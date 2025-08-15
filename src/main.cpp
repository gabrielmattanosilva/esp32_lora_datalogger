/**
 * @file main.cpp
 * @brief Receptor LoRa com publicação opcional no ThingSpeak via Wi-Fi.
 * @details
 *  - RX por interrupção (LoRa.onReceive) com buffer 1-slot (sem fila).
 *  - Processa o pacote imediatamente (decrypt + parse + logs).
 *  - Envia ao ThingSpeak somente se Wi-Fi estiver conectado; caso contrário, descarta.
 *  - Field1 envia -1.0 quando irradiance==0xFFFF (erro).
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
#include "serial_log.h"

static const char *TAG = "MAIN";

/**
 * @brief Imprime o payload decodificado em formato legível.
 * @param p Ponteiro para PayloadPacked válido.
 */
static void print_decoded(const PayloadPacked *p)
{
    const bool irr_error = (p->irradiance == 0xFFFF);
    const float batt_V = p->battery_voltage / 1000.0f;
    const float temp_C = p->internal_temperature / 10.0f;

    LOGI(TAG, "---- Pacote decodificado ----");
    if (irr_error)
    {
        LOGW(TAG, "Irradiancia : ERRO (0xFFFF)");
    }
    else
    {
        LOGI(TAG, "Irradiancia : %u W/m^2", (unsigned)p->irradiance);
    }
    LOGI(TAG, "Bateria     : %.3f V", batt_V);
    LOGI(TAG, "Temp. int.  : %.1f C", temp_C);
    LOGI(TAG, "Timestamp   : %lu s", (unsigned long)p->timestamp);
    LOGI(TAG, "Checksum    : 0x%02X", p->checksum);
    LOGI(TAG, "-----------------------------");
}

// Buffer 1-slot preenchido na interrupção
static volatile bool g_pkt_ready = false; ///< Indica que há pacote pendente.
static volatile uint16_t g_pkt_len = 0;   ///< Tamanho do pacote pendente.
static volatile int16_t g_pkt_rssi = 0;   ///< RSSI do pacote.
static volatile float g_pkt_snr = 0;      ///< SNR do pacote.
static uint8_t g_pkt_buf[128];            ///< Buffer do pacote (máx 128B).

/**
 * @brief Callback chamado pelo driver LoRa ao receber um pacote (DIO0).
 * @param packetSize Número de bytes disponibilizados pelo rádio.
 * @note Leve: apenas copia bytes e atualiza metadados. Não faz decrypt/HTTP aqui.
 */
static void lora_on_rx_isr(int packetSize)
{
    if (packetSize <= 0)
        return;
    if (packetSize > (int)sizeof(g_pkt_buf))
        packetSize = sizeof(g_pkt_buf);

    int n = 0;
    while (LoRa.available() && n < packetSize)
    {
        g_pkt_buf[n++] = (uint8_t)LoRa.read();
    }
    g_pkt_len = (uint16_t)n;
    g_pkt_rssi = LoRa.packetRssi();
    g_pkt_snr = LoRa.packetSnr();
    g_pkt_ready = true; // sobrescreve anterior se não processado
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
    }

    LOGI(TAG, "Iniciando datalogger...");
    LOGI(TAG, "AES-128-CBC; Formato: IV(16) || CT(16*n) -> Payload(11B)");
    crypto_init(AES_KEY);

    // Wi-Fi: reconexão não-bloqueante
    wifi_begin(WIFI_SSID, WIFI_PASSWORD);
    wifi_force_reconnect();

    // LoRa
    if (!lora_begin())
    {
        LOGE(TAG, "Falha ao inicializar LoRa. Verifique conexoes/pinos.");
        for (;;)
        {
            delay(1000);
        }
    }
    LoRa.onReceive(lora_on_rx_isr);
    LoRa.receive();
    LOGI(TAG, "LoRa OK. RX imediato por interrupcao habilitado.");
}

void loop()
{
    // Mantém reconexão Wi-Fi (sem bloquear)
    wifi_tick(millis());

    // Copia atômica do pacote pendente para buffers locais
    uint8_t local_buf[128];
    uint16_t local_len = 0;
    int16_t local_rssi = 0;
    float local_snr = 0.0f;

    noInterrupts();
    if (g_pkt_ready)
    {
        local_len = g_pkt_len;
        local_rssi = g_pkt_rssi;
        local_snr = g_pkt_snr;
        for (uint16_t i = 0; i < local_len; ++i)
            local_buf[i] = g_pkt_buf[i];
        g_pkt_ready = false; // consome (sem fila/armazenamento)
    }
    interrupts();

    if (local_len == 0)
    {
        delay(1);
        return;
    }

    LOGI(TAG, "RX [%u B]  RSSI=%d  SNR=%.1f", (unsigned)local_len, (int)local_rssi, local_snr);
    LOG_HEXDUMP(TAG, 'D', local_buf, local_len);

    // Validação básica: IV(16) + CT(>=16 e múltiplo de 16)
    if (local_len < 32u)
    {
        LOGW(TAG, "Pacote curto (IV16 + CT16+). DESCARTADO.");
        return;
    }
    const uint8_t *iv = &local_buf[0];
    const uint8_t *ct = &local_buf[16];
    uint16_t ct_len = (uint16_t)(local_len - 16u);
    if ((ct_len % 16u) != 0u)
    {
        LOGW(TAG, "Ciphertext nao multiplo de 16. DESCARTADO.");
        return;
    }

    // Decrypt + unpad
    uint8_t plain[128];
    size_t plain_len = 0;
    if (!crypto_decrypt(ct, (size_t)ct_len, iv, plain, &plain_len))
    {
        LOGW(TAG, "AES fail. DESCARTADO.");
        return;
    }
    if (plain_len != sizeof(PayloadPacked))
    {
        LOGW(TAG, "Tamanho apos unpad invalido (%u). DESCARTADO.", (unsigned)plain_len);
        return;
    }

    // Parse
    PayloadPacked p;
    if (!lora_parse_payload(plain, plain_len, &p))
    {
        LOGW(TAG, "Payload invalido (checksum/estrutura). DESCARTADO.");
        return;
    }

    // Human readable
    print_decoded(&p);

    // Campos ThingSpeak (Field1 = -1 em erro de irradiância)
    const bool irr_error = (p.irradiance == 0xFFFF);
    float irr_Wm2 = irr_error ? -1.0f : (float)p.irradiance;
    float batt_V = p.battery_voltage / 1000.0f;
    float temp_C = p.internal_temperature / 10.0f;

    // Envia somente se Wi-Fi estiver conectado; caso contrário, descarta (sem reenvio)
    if (wifi_is_connected())
    {
        bool ok = thingspeak_update(THINGSPEAK_API_KEY, irr_Wm2, batt_V, temp_C, p.timestamp);
        if (ok)
            LOGI(TAG, "ThingSpeak: OK");
        else
            LOGE(TAG, "ThingSpeak: FALHA (nao sera re-enviado)");
    }
    else
    {
        LOGW(TAG, "ThingSpeak: sem Wi-Fi -> pacote DESCARTADO.");
    }
}
