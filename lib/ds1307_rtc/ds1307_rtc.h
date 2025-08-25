/**
 * @file ds1307_rtc.cpp
 * @brief Cabeçalho para a integração com o RTC DS1307 usando RTClib para sincronizar o RTC interno do ESP32.
 */

#ifndef DS1307_RTC_H
#define DS1307_RTC_H

#include <stdbool.h>

bool ds1307_rtc_sync_at_boot(void);

#endif /* DS1307_RTC_H */
