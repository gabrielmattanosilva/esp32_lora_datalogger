/**
 * @file sd_card.h
 * @brief Logger em cartão SD com rotação diária de arquivo.
 *
 * - Cria arquivo no formato YYYYMMDD_HHMMSS.log usando o RTC interno do ESP32.
 * - Rotaciona automaticamente quando muda o dia (00:00:00).
 * - Espelha mensagens de log (via logger.h) para o SD.
 * - Compartilha o mesmo barramento SPI do LoRa (CS dedicados).
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/** Inicializa o SD. Retorna true em sucesso. */
bool sdcard_begin();

/** Deve ser chamado periodicamente para checar mudança de dia e rotacionar arquivo. */
void sdcard_tick_rotate();

/** Escreve uma linha formatada (printf-like) no arquivo atual. */
void sdcard_printf(const char *fmt, ...) __attribute__((format(printf,1,2)));

/** Versão que recebe va_list (usada pelas macros de log). */
void sdcard_vprintf(const char *fmt, va_list ap);

/** Força flush no arquivo atual. */
void sdcard_flush();

/** Encerra arquivo/montagem. */
void sdcard_end();

#endif /* SD_CARD_H */
