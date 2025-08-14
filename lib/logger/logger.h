#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <stdint.h>
#include <esp_log.h>

void logger_init_default(void);                 // seta nível global e imprime banner
void logger_set_level(const char* tag, esp_log_level_t level);

// hexdump padronizado (quebra em linhas de 16B)
void logger_hexdump(const char* tag, esp_log_level_t level, const uint8_t* buf, size_t len);

#endif /* LOGGER_H */
