/**
 * @file main.cpp
 * @brief Receptor LoRa com logger, sincronização do RTC interno via DS1307,
 *        epoch0 fallback e rotação diária no SD.
 *
 * Log format: "YYYY/MM/DD HH:MM:SS.mmm [TAG] mensagem"
 * RTC: no boot força epoch0; em seguida tenta sincronizar a partir do DS1307.
 * SD: cria /YYYYMMDD_HHMMSS.log e rotaciona ao virar o dia (ver sd_card.cpp).
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "pins.h"
#include "credentials.h"
#include "utils.h"
#include "crypto.h"
#include "sx1278_lora.h"
#include "wifi_manager.h"
#include "thingspeak_client.h"

#include "sd_card.h"     // espelhamento opcional (se SD presente)
#include "logger.h"      // logger simples com timestamp
#include "ds1307_rtc.h"  // <<< NOVO: sync do RTC interno a partir do DS1307

static const char *TAG = "MAIN";

/* ---------- Buffer preenchido no ISR (1 escritor) ---------- */
static volatile bool     g_pkt_ready = false;
static volatile uint16_t g_pkt_len   = 0;
static volatile int16_t  g_pkt_rssi  = 0;
static volatile float    g_pkt_snr   = 0.0f;
static uint8_t           g_pkt_buf[128];

/* ---------- Protótipos ---------- */
static void print_decoded(const PayloadPacked *p);
static void on_lora_rx_isr(int packetSize);

/* ---------- ISR DIO0: leve (copia bytes e metadata) ---------- */
static void on_lora_rx_isr(int packetSize)
{
    if (packetSize <= 0) return;
    if (packetSize > (int)sizeof(g_pkt_buf)) packetSize = sizeof(g_pkt_buf);

    int n = 0;
    while (LoRa.available() && n < packetSize) {
        g_pkt_buf[n++] = (uint8_t)LoRa.read();
    }
    g_pkt_len   = (uint16_t)n;
    g_pkt_rssi  = LoRa.packetRssi();
    g_pkt_snr   = LoRa.packetSnr();
    g_pkt_ready = true; // sobrescreve se o anterior não foi processado
}

/* ---------- Log humano do payload ---------- */
static void print_decoded(const PayloadPacked *p)
{
    const bool irr_error = (p->irradiance == 0xFFFF);
    const float batt_V   = p->battery_voltage / 1000.0f;
    const float temp_C   = p->internal_temperature / 10.0f;

    LOG(TAG, "---- Pacote decodificado ----");
    if (irr_error)  LOG(TAG, "Irradiancia : ERRO (0xFFFF)");
    else            LOG(TAG, "Irradiancia : %u W/m^2", (unsigned)p->irradiance);

    LOG(TAG, "Bateria     : %.3f V", batt_V);
    LOG(TAG, "Temp. int.  : %.1f C", temp_C);
    LOG(TAG, "Timestamp   : %lu s", (unsigned long)p->timestamp);
    LOG(TAG, "Checksum    : 0x%02X", p->checksum);
    LOG(TAG, "-----------------------------");
}

void setup()
{
    // 0) RTC sempre em epoch 0 no boot (1970-01-01 00:00:00.000 UTC)
    logger_init_epoch0();

    // 1) Inicializa SD (se falhar, seguimos só com Serial)
    sdcard_begin();

    // 2) Inicia logger (Serial + espelho no SD)
    logger_begin();

    // 0.1) <<< NOVO >>> Sincroniza RTC interno a partir do DS1307 (se presente/valido)
    //     Faça ANTES do sdcard_begin() para que o nome do arquivo já use data/hora reais.
    if (ds1307_rtc_sync_at_boot()) {
        LOG(TAG, "RTC interno sincronizado a partir do DS1307");
    } else {
        LOG(TAG, "DS1307 ausente/invalido, mantendo epoch0 ate ter hora valida");
    }

    // 3) Crypto
    crypto_init(AES_KEY);

    // 4) Wi‑Fi (não bloqueante)
    wifi_begin(WIFI_SSID, WIFI_PASSWORD);
    wifi_force_reconnect();

    // 5) LoRa
    if (!lora_begin()) {
        LOG("LORA", "Falha ao inicializar LoRa");
        for (;;) { delay(1000); }
    }
    LoRa.onReceive(on_lora_rx_isr);
    LoRa.receive();
    LOG(TAG, "LoRa inicializado, aguardando pacotes...");
}

void loop()
{
    // Rotação diária do arquivo de log no SD
    sdcard_tick_rotate();

    // Máquina de estados do Wi‑Fi (non‑blocking)
    wifi_tick(millis());

    // Captura atômica do slot do ISR
    uint8_t  local_buf[128];
    uint16_t local_len = 0;
    int16_t  local_rssi = 0;
    float    local_snr  = 0.0f;

    noInterrupts();
    if (g_pkt_ready) {
        local_len  = g_pkt_len;
        local_rssi = g_pkt_rssi;
        local_snr  = g_pkt_snr;
        for (uint16_t i = 0; i < local_len; ++i) local_buf[i] = g_pkt_buf[i];
        g_pkt_ready = false;
    }
    interrupts();

    if (local_len == 0) {
        delay(1);
        return;
    }

    // Logs brutos do pacote
    LOG("LORA", "RX [%u B]  RSSI=%d  SNR=%.1f", (unsigned)local_len, (int)local_rssi, local_snr);
    LOGHEX("LORA", local_buf, local_len);

    // Validação básica: IV(16) + CT(>=16 e multiplo de 16)
    if (local_len < 32u) {
        LOG(TAG, "Pacote curto (IV16 + CT16+), DESCARTADO");
        sdcard_flush();
        return;
    }
    const uint8_t *iv = &local_buf[0];
    const uint8_t *ct = &local_buf[16];
    const uint16_t ct_len = (uint16_t)(local_len - 16u);
    if ((ct_len % 16u) != 0u) {
        LOG(TAG, "Ciphertext nao multiplo de 16, DESCARTADO");
        sdcard_flush();
        return;
    }

    // Decrypt + unpad
    uint8_t plain[128];
    size_t  plain_len = 0;
    if (!crypto_decrypt(ct, (size_t)ct_len, iv, plain, &plain_len)) {
        LOG(TAG, "AES fail, DESCARTADO");
        sdcard_flush();
        return;
    }
    if (plain_len != sizeof(PayloadPacked)) {
        LOG(TAG, "Tamanho apos unpad invalido (%u), DESCARTADO", (unsigned)plain_len);
        sdcard_flush();
        return;
    }

    // Parse / validação do payload
    PayloadPacked p;
    if (!lora_parse_payload(plain, plain_len, &p)) {
        LOG(TAG, "Payload invalido (checksum/estrutura), DESCARTADO");
        sdcard_flush();
        return;
    }

    // Log humano
    print_decoded(&p);

    // Envio opcional ao ThingSpeak
    const bool irr_error = (p.irradiance == 0xFFFF);
    const float irr_Wm2  = irr_error ? -1.0f : (float)p.irradiance;
    const float batt_V   = p.battery_voltage / 1000.0f;
    const float temp_C   = p.internal_temperature / 10.0f;

    if (wifi_is_connected()) {
        bool ok = thingspeak_update(THINGSPEAK_API_KEY, irr_Wm2, batt_V, temp_C, p.timestamp);
        if (ok)  LOG("TS", "envio OK");
        else     LOG("TS", "FALHA no envio");
    } else {
        LOG("TS", "sem conexao Wi-Fi, pacote NAO enviado");
    }

    // Flush gentil (sd_card.cpp já faz flush periódico)
    sdcard_flush();
}
