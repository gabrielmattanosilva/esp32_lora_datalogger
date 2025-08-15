/**
 * @file thingspeak_client.h
 * @brief Cabeçalho para as funções de cliente ThingSpeak (HTTP).
 */

#ifndef THINGSPEAK_CLIENT_H
#define THINGSPEAK_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

bool thingspeak_update(const char *api_key,
                       float field1_irradiance_Wm2,
                       float field2_batt_V,
                       float field3_temp_C,
                       uint32_t field4_timestamp_s);

#endif /* THINGSPEAK_CLIENT_H */
