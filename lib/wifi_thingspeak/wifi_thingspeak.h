#ifndef WIFI_THINGSPEAK_H
#define WIFI_THINGSPEAK_H

#include <stdint.h>
#include <stdbool.h>

// Inicializa Wi‑Fi (bloqueia até conectar)
bool wifi_connect_blocking(const char* ssid, const char* pass, uint32_t timeout_ms);

// Publica 4 campos no ThingSpeak (respeita rate-limit interno)
bool thingspeak_update4(const char* api_key,
                        float field1_irradiance_Wm2,
                        float field2_batt_V,
                        float field3_temp_C,
                        uint32_t field4_timestamp_s);

#endif /* WIFI_THINGSPEAK_H */
