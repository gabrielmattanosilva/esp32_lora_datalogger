/**
 * @file ds1307_rtc.h
 * @brief Sincroniza o RTC interno do ESP32 a partir de um DS1307 via I2C.
 */
#ifndef DS1307_RTC_H
#define DS1307_RTC_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

bool ds1307_rtc_begin(void);

/**
 * @brief Sincroniza o RTC interno **apenas se** o DS1307 estiver rodando.
 * @return true se sincronizou; false caso contrário (ausente, parado ou data inválida).
 */
bool ds1307_rtc_sync_at_boot(void);

/* utilitários opcionais */
bool ds1307_rtc_set_epoch(time_t epoch);
bool ds1307_rtc_get_epoch(time_t *out_epoch);

/** Consulta se o relógio do DS1307 está rodando (bit CH limpo). */
bool ds1307_rtc_is_running(bool *out_running);

#endif /* DS1307_RTC_H */
