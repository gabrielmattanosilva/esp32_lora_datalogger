#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Payload compacto (11 bytes) — deve casar com o transmissor */
typedef struct __attribute__((packed)) {
    uint16_t irradiance;           /* W/m² (0..2000, 0xFFFF = erro) */
    uint16_t battery_voltage;      /* mV */
    int16_t  internal_temperature; /* °C ×10 */
    uint32_t timestamp;            /* s */
    uint8_t  checksum;             /* soma 8-bit dos 10 primeiros bytes */
} PayloadPacked;

uint8_t  utils_checksum8(const uint8_t *data, size_t len);
uint16_t utils_rd_le_u16(const uint8_t *b);
int16_t  utils_rd_le_i16(const uint8_t *b);
uint32_t utils_rd_le_u32(const uint8_t *b);

/* Converte o buffer decifrado (11B) em PayloadPacked e faz sanity checks */
bool utils_parse_payload(const uint8_t *buf, size_t len, PayloadPacked *out);

/* Hexdump simples (para debug) */
void utils_hexdump(const uint8_t *buf, int len);

#endif /* UTILS_H */
