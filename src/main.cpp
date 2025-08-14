#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <esp_log.h>

#include "pins.h"
#include "credentials.h"
#include "logger.h"
#include "utils.h"
#include "crypto.h"
#include "sx1278_lora.h"

#include "wifi_manager.h"
#include "thingspeak_client.h"

static const char* TAG = "MAIN";

// ---------- Impressão dos dados decodificados ----------
static void print_decoded(const PayloadPacked *p) {
    const bool  irr_error = (p->irradiance == 0xFFFF);
    const float batt_V    = p->battery_voltage / 1000.0f;
    const float temp_C    = p->internal_temperature / 10.0f;

    ESP_LOGI(TAG, "---- Pacote decodificado ----");
    if (irr_error) {
        ESP_LOGW(TAG, "Irradiancia : ERRO (0xFFFF)");
    } else {
        ESP_LOGI(TAG, "Irradiancia : %u W/m^2", (unsigned)p->irradiance);
    }
    ESP_LOGI(TAG, "Bateria     : %.3f V", batt_V);
    ESP_LOGI(TAG, "Temp. int.  : %.1f C", temp_C);
    ESP_LOGI(TAG, "Timestamp   : %lu s", (unsigned long)p->timestamp);
    ESP_LOGI(TAG, "Checksum    : 0x%02X", p->checksum);
    ESP_LOGI(TAG, "-----------------------------");
}

// ---------- Buffer único (1-slot) preenchido na interrupção ----------
static volatile bool     g_pkt_ready = false;
static volatile uint16_t g_pkt_len   = 0;
static volatile int16_t  g_pkt_rssi  = 0;
static volatile float    g_pkt_snr   = 0;
static uint8_t           g_pkt_buf[128];

// Callback chamado quando o SX1278 sinaliza recepção (DIO0)
static void lora_on_rx_isr(int packetSize)
{
    if (packetSize <= 0) return;
    if (packetSize > (int)sizeof(g_pkt_buf)) packetSize = sizeof(g_pkt_buf);

    int n = 0;
    while (LoRa.available() && n < packetSize) {
        g_pkt_buf[n++] = (uint8_t)LoRa.read();
    }
    g_pkt_len  = (uint16_t)n;
    g_pkt_rssi = LoRa.packetRssi();
    g_pkt_snr  = LoRa.packetSnr();

    g_pkt_ready = true; // sobrescreve anterior se ainda não processou
}

void setup() {
    // Inicializa UART apenas para garantir console visível; logs saem via esp_log
    Serial.begin(115200);
    delay(50);

    logger_init_default(); // nível global e banner

    ESP_LOGI(TAG, "AES-128-CBC; Formato: IV(16) || CT(16*n) -> Payload(11B)");
    crypto_init(AES_KEY);

    // Wi-Fi: reconexão não-bloqueante em background (com logs de transição)
    wifi_init(WIFI_SSID, WIFI_PASSWORD);
    wifi_force_reconnect();

    // LoRa
    if (!lora_begin()) {
        ESP_LOGE(TAG, "Falha ao inicializar LoRa. Verifique conexoes/pinos.");
        for (;;) { delay(1000); }
    }
    LoRa.onReceive(lora_on_rx_isr);
    LoRa.receive();
    ESP_LOGI(TAG, "LoRa OK. RX imediato por interrupcao habilitado.");
}

void loop() {
    // Mantém retry de Wi-Fi (não bloqueante; com logs)
    wifi_tick(millis());

    // Copia atômica do 1-slot para buffers locais
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
        g_pkt_ready = false; // consome (sem fila/armazenamento)
    }
    interrupts();

    if (local_len == 0) {
        delay(1);
        return;
    }

    ESP_LOGI(TAG, "RX [%u B]  RSSI=%d  SNR=%.1f", (unsigned)local_len, (int)local_rssi, local_snr);
    logger_hexdump(TAG, ESP_LOG_DEBUG, local_buf, local_len);

    // Validação básica: IV(16) + CT(>=16 e múltiplo de 16)
    if (local_len < 32u) {
        ESP_LOGW(TAG, "Pacote curto (IV16 + CT16+). DESCARTADO.");
        return;
    }
    const uint8_t *iv = &local_buf[0];
    const uint8_t *ct = &local_buf[16];
    uint16_t ct_len = (uint16_t)(local_len - 16u);
    if ((ct_len % 16u) != 0u) {
        ESP_LOGW(TAG, "Ciphertext nao multiplo de 16. DESCARTADO.");
        return;
    }

    uint8_t plain[128]; size_t plain_len = 0;
    if (!crypto_decrypt(ct, (size_t)ct_len, iv, plain, &plain_len)) {
        ESP_LOGW(TAG, "AES fail. DESCARTADO.");
        return;
    }
    if (plain_len != sizeof(PayloadPacked)) {
        ESP_LOGW(TAG, "Tamanho apos unpad invalido (%u). DESCARTADO.", (unsigned)plain_len);
        return;
    }

    PayloadPacked p;
    if (!utils_parse_payload(plain, plain_len, &p)) {
        ESP_LOGW(TAG, "Payload invalido (checksum/estrutura). DESCARTADO.");
        return;
    }

    print_decoded(&p);

    // Campos ThingSpeak (Field1 = -1 em erro)
    const bool irr_error = (p.irradiance == 0xFFFF);
    float irr_Wm2 = irr_error ? -1.0f : (float)p.irradiance;
    float batt_V  = p.battery_voltage / 1000.0f;
    float temp_C  = p.internal_temperature / 10.0f;

    if (wifi_is_connected()) {
        bool ok = thingspeak_update4(THINGSPEAK_API_KEY, irr_Wm2, batt_V, temp_C, p.timestamp);
        if (ok) ESP_LOGI(TAG, "ThingSpeak: OK");
        else    ESP_LOGE(TAG, "ThingSpeak: FALHA (nao sera re-enviado)");
    } else {
        ESP_LOGW(TAG, "ThingSpeak: sem Wi-Fi -> pacote DESCARTADO.");
    }
}
