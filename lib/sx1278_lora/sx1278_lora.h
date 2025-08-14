#ifndef SX1278_LORA_H
#define SX1278_LORA_H

#include <stdint.h>
#include <stdbool.h>

/* Inicializa rádio LoRa (433 MHz), SyncWord 0xA5. Retorna true se OK. */
bool lora_init(void);

/* Bloqueante: tenta ler um pacote para buf (até max_len).
 * Retorna número de bytes lidos (>0), e sai com *out_rssi, *out_snr.
 * Se nada disponível, retorna 0.
 */
int  lora_read_packet(uint8_t *buf, int max_len, int *out_rssi, float *out_snr);

#endif /* SX1278_LORA_H */
