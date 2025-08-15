/**
 * @file sx1278_lora.cpp
 * @brief Implementação das funções para comunicação LoRa com o módulo SX1278.
 */

#include "sx1278_lora.h"
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "serial_log.h"
#include "pins.h"
#include "crypto.h"
#include "credentials.h"
#include "utils.h"

static const char *TAG = "LORA";

/****************************** Funções públicas ******************************/

/**
 * @brief Inicializa o módulo LoRa.
 * @return Verdadeiro se a inicialização foi bem-sucedida, falso caso contrário.
 */
bool lora_begin(void)
{
    pinMode(SPI_SS, OUTPUT);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
    LoRa.setSPI(SPI);
    LoRa.setPins(SPI_SS, SX1278_RST, SX1278_DIO0);

    if (!LoRa.begin(433E6))
    {
        LOGE(TAG, "begin(433E6) falhou");
        return false;
    }

    LoRa.setSyncWord(0xA5);

    LOGI(TAG, "inicializado: freq=433MHz, sync=0xA5");
    return true;
}

/**
 * @brief Lê um pacote LoRa recebido e armazena no buffer fornecido.
 * @param buf Ponteiro para o buffer onde os dados recebidos serão armazenados.
 * @param max_len Tamanho máximo do buffer (em bytes).
 * @param out_rssi Ponteiro opcional para armazenar o valor do RSSI do pacote (dBm).
 * @param out_snr Ponteiro opcional para armazenar o valor do SNR do pacote (dB).
 * @return Número de bytes efetivamente lidos (0 se nenhum pacote foi recebido).
 */
uint32_t lora_read_packet(uint8_t *buf, uint16_t max_len, int16_t *out_rssi, float *out_snr)
{
    if (!buf || max_len == 0)
    {
        return 0;
    }

    int packetSize = LoRa.parsePacket();

    if (packetSize <= 0)
    {
        return 0;
    }

    uint32_t n = 0;

    while (LoRa.available() && n < max_len)
    {
        buf[n++] = (uint8_t)LoRa.read();
    }

    if (out_rssi)
    {
        *out_rssi = LoRa.packetRssi();
    }

    if (out_snr)
    {
        *out_snr = LoRa.packetSnr();
    }

    LOGD(TAG, "RX %u bytes (RSSI=%d, SNR=%.1f)", (unsigned)n,
         out_rssi ? *out_rssi : 0, out_snr ? *out_snr : 0.0f);
    return n;
}

/**
 * @brief Faz o parse de um payload bruto para a estrutura PayloadPacked.
 * @param buf Ponteiro para o buffer com os dados recebidos.
 * @param len Tamanho do buffer (deve ser exatamente sizeof(PayloadPacked)).
 * @param out Ponteiro para a estrutura PayloadPacked a ser preenchida.
 * @return true se o payload for válido e o parse bem-sucedido, false caso contrário.
 */
bool lora_parse_payload(const uint8_t *buf, size_t len, PayloadPacked *out)
{
    if (!buf || !out || len != sizeof(PayloadPacked))
    {
        LOGE(TAG, "parse_payload: tamanho invalido (len=%u, esperado=%u)",
             (unsigned)len, (unsigned)sizeof(PayloadPacked));
        return false;
    }

    uint8_t calc = utils_checksum8(buf, sizeof(PayloadPacked) - 1);

    if (calc != buf[sizeof(PayloadPacked) - 1])
    {
        LOGW(TAG, "checksum invalido (calc=0x%02X, rx=0x%02X)", calc, buf[10]);
        return false;
    }

    out->irradiance = utils_rd_le_u16(&buf[0]);
    out->battery_voltage = utils_rd_le_u16(&buf[2]);
    out->internal_temperature = utils_rd_le_i16(&buf[4]);
    out->timestamp = utils_rd_le_u32(&buf[6]);
    out->checksum = buf[10];
    return true;
}