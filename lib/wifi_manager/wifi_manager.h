/**
 * @file wifi_manager.h
 * @brief Cabeçalho para as funções do gerenciador de Wi-Fi.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

void wifi_begin(const char *ssid, const char *pass);
bool wifi_is_connected(void);
void wifi_force_reconnect(void);
void wifi_tick(uint32_t now_ms);

#endif /* WIFI_MANAGER_H */
