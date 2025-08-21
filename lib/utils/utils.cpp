/**
 * @file utils.cpp
 * @brief Implementação de funções utilitárias.
 */

static const char *TAG = "UTILS";

#include "utils.h"
#include "logger.h"

/****************************** Funções públicas ******************************/

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
