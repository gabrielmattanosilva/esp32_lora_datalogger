#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

/** Define o RTC interno para epoch 0 (1970-01-01 00:00:00.000 UTC). */
void logger_init_epoch0();

/** Inicia o logger.
 *  @param init_serial  true para chamar Serial.begin(baud)
 *  @param serial_baud  baud da Serial (se init_serial==true)
 */
void logger_begin();

/** Log genérico (printf-like). Uso: LOG("MAIN", "valor=%d", v); */
void logger_log(const char *tag, const char *fmt, ...) __attribute__((format(printf,2,3)));

/** Hexdump: 16 bytes por linha, com mesmo prefixo de timestamp + [TAG]. */
void logger_hexdump(const char *tag, const uint8_t *buf, size_t len);

/* Macros de conveniência */
#define LOG(TAG, fmt, ...)   logger_log((TAG), (fmt), ##__VA_ARGS__)
#define LOGHEX(TAG, B, L)    logger_hexdump((TAG), (const uint8_t*)(B), (size_t)(L))
