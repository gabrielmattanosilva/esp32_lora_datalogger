/**
 * @file sx1278_lora.h
 * @brief Cabeçalho para as funções de comunicação LoRa com o módulo SX1278.
 */

#ifndef SX1278_LORA_H
#define SX1278_LORA_H

#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Estrutura para o payload compactado enviado via LoRa.
 */
typedef struct __attribute__((packed))
{
    uint16_t irradiance;          /* W/^2 (0..2000, 0xFFFF = erro)        */
    uint16_t battery_voltage;     /* mV                                   */
    int16_t internal_temperature; /* °C*10                                */
    uint32_t timestamp;           /* s                                    */
    uint8_t checksum;             /* soma 8-bit (dos 10 bytes anteriores) */
} PayloadPacked;
_Static_assert(sizeof(PayloadPacked) == 11, "Payload deve ter 11 bytes");

bool lora_begin(void);
uint32_t lora_read_packet(uint8_t *buf, uint16_t max_len, int16_t *out_rssi, float *out_snr);
bool lora_parse_payload(const uint8_t *buf, size_t len, PayloadPacked *out);

#endif /* SX1278_LORA_H */
