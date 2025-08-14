/**
 * @file utils.h
 * @brief Cabeçalho para funções utilitárias.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sx1278_lora.h"

uint8_t utils_checksum8(const uint8_t *data, size_t len);
uint16_t utils_rd_le_u16(const uint8_t *b);
int16_t utils_rd_le_i16(const uint8_t *b);
uint32_t utils_rd_le_u32(const uint8_t *b);
bool utils_parse_payload(const uint8_t *buf, size_t len, PayloadPacked *out);
void utils_hexdump(const uint8_t *buf, int len);

#endif /* UTILS_H */
