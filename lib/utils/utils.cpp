#include "utils.h"
#include <Arduino.h>

uint8_t utils_checksum8(const uint8_t *data, size_t len) {
    uint8_t s = 0;
    for (size_t i = 0; i < len; ++i) s += data[i];
    return s;
}

uint16_t utils_rd_le_u16(const uint8_t *b) {
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}
int16_t utils_rd_le_i16(const uint8_t *b) {
    return (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}
uint32_t utils_rd_le_u32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

bool utils_parse_payload(const uint8_t *buf, size_t len, PayloadPacked *out) {
    if (!buf || !out || len != sizeof(PayloadPacked)) return false;
    /* valida checksum (soma dos 10 primeiros bytes) */
    uint8_t calc = utils_checksum8(buf, sizeof(PayloadPacked) - 1);
    if (calc != buf[sizeof(PayloadPacked) - 1]) return false;

    out->irradiance           = utils_rd_le_u16(&buf[0]);
    out->battery_voltage      = utils_rd_le_u16(&buf[2]);
    out->internal_temperature = utils_rd_le_i16(&buf[4]);
    out->timestamp            = utils_rd_le_u32(&buf[6]);
    out->checksum             = buf[10];
    return true;
}

void utils_hexdump(const uint8_t *buf, int len) {
    if (!buf || len <= 0) return;
    for (int i = 0; i < len; ++i) {
        if (i && (i % 16 == 0)) Serial.println();
        if (buf[i] < 16) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}
