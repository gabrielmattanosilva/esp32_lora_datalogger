#include "wifi_thingspeak.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

bool wifi_connect_blocking(const char *ssid, const char *pass, uint32_t timeout_ms)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - t0 > timeout_ms)
        {
            return false;
        }
        delay(250);
    }
    return true;
}

static bool http_post_form(const String &url, const String &body)
{
    HTTPClient http;
    WiFiClient client;

    if (!http.begin(client, url))
    {
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(body);
    String payload = http.getString(); // opcional para debug
    http.end();
    return (code == 200) && payload.length() > 0;
}

bool thingspeak_update4(const char *api_key,
                        float field1_irradiance_Wm2,
                        float field2_batt_V,
                        float field3_temp_C,
                        uint32_t field4_timestamp_s)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "api_key=%s&field1=%.1f&field2=%.3f&field3=%.1f&field4=%lu",
             api_key,
             field1_irradiance_Wm2,
             field2_batt_V,
             field3_temp_C,
             (unsigned long)field4_timestamp_s);

    return http_post_form("http://api.thingspeak.com/update", String(buf));
}
