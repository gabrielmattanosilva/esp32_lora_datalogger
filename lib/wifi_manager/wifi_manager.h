#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

void wifi_init(const char* ssid, const char* pass);
void wifi_tick(uint32_t now_ms);
void wifi_force_reconnect(void);

bool     wifi_is_connected(void);
const char* wifi_ip_str(void);
int32_t  wifi_rssi(void);

#endif /* WIFI_MANAGER_H */
