/**
 * @file utils.cpp
 * @brief Implementação de funções utilitárias.
 */

static const char* TAG = "UTILS";

#include "utils.h"
#include <Arduino.h>
#include "logger.h"
#include <esp_log.h>

/**
 * @brief Calcula uma soma de verificação 8-bit para os dados fornecidos.
 * @param data Ponteiro para os dados.
 * @param len Tamanho dos dados.
 * @return Soma de verificação 8-bit.
 */
uint8_t utils_checksum8(const uint8_t *data, size_t len)
{
    uint8_t s = 0;

    for (size_t i = 0; i < len; ++i)
    {
        s += data[i];
    }

    return s;
}

/**
 * @brief Lê um valor unsigned 16-bit (uint16_t) em formato little-endian.
 * @param b Ponteiro para os dois bytes que compõem o valor.
 * @return Valor convertido para uint16_t.
 */
uint16_t utils_rd_le_u16(const uint8_t *b)
{
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

/**
 * @brief Lê um valor signed 16-bit (int16_t) em formato little-endian.
 * @param b Ponteiro para os dois bytes que compõem o valor.
 * @return Valor convertido para int16_t.
 */
int16_t utils_rd_le_i16(const uint8_t *b)
{
    return (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

/**
 * @brief Lê um valor unsigned 32-bit (uint32_t) em formato little-endian.
 * @param b Ponteiro para os quatro bytes que compõem o valor.
 * @return Valor convertido para uint32_t.
 */
uint32_t utils_rd_le_u32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

/**
 * @brief Faz o parse de um payload bruto para a estrutura PayloadPacked.
 * @param buf Ponteiro para o buffer com os dados recebidos.
 * @param len Tamanho do buffer (deve ser exatamente sizeof(PayloadPacked)).
 * @param out Ponteiro para a estrutura PayloadPacked a ser preenchida.
 * @return true se o payload for válido e o parse bem-sucedido, false caso contrário.
 */
bool utils_parse_payload(const uint8_t *buf, size_t len, PayloadPacked *out)
{
    if (!buf || !out || len != sizeof(PayloadPacked))
    {
        ESP_LOGE(TAG, "parse_payload: tamanho invalido (len=%u, esperado=%u)",
                 (unsigned)len, (unsigned)sizeof(PayloadPacked));
        return false;
    }

    uint8_t calc = utils_checksum8(buf, sizeof(PayloadPacked) - 1);
    if (calc != buf[sizeof(PayloadPacked) - 1])
    {
        ESP_LOGW(TAG, "checksum invalido (calc=0x%02X, rx=0x%02X)", calc, buf[10]);
        return false;
    }

    out->irradiance          = utils_rd_le_u16(&buf[0]);
    out->battery_voltage     = utils_rd_le_u16(&buf[2]);
    out->internal_temperature= utils_rd_le_i16(&buf[4]);
    out->timestamp           = utils_rd_le_u32(&buf[6]);
    out->checksum            = buf[10];
    return true;
}

/**
 * @brief Imprime um buffer de dados em formato hexadecimal.
 * @param buf Ponteiro para o buffer de dados.
 * @param len Número de bytes no buffer.
 */
void utils_hexdump(const uint8_t *buf, int len)
{
    if (!buf || len <= 0) return;
    logger_hexdump(TAG, ESP_LOG_DEBUG, buf, (size_t)len);
}
