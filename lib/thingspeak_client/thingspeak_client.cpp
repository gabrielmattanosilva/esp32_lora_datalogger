#include "thingspeak_client.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_log.h>

static const char* TAG = "TS";

static bool http_post_form(const String &url, const String &body)
{
    if (!wifi_is_connected()) return false;

    HTTPClient http;
    WiFiClient client;

    if (!http.begin(client, url)) {
        ESP_LOGE(TAG, "http.begin() falhou");
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int code = http.POST(body);
    String payload = http.getString(); // opcional
    http.end();

    ESP_LOGI(TAG, "HTTP %d, payload_len=%d", code, payload.length());
    return (code == 200) && payload.length() > 0;
}

bool thingspeak_update4(const char* api_key,
                        float field1_irradiance_Wm2,
                        float field2_batt_V,
                        float field3_temp_C,
                        uint32_t field4_timestamp_s)
{
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "sem Wi-Fi. Nao enviado.");
        return false;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "api_key=%s&field1=%.1f&field2=%.3f&field3=%.1f&field4=%lu",
             api_key, field1_irradiance_Wm2, field2_batt_V, field3_temp_C,
             (unsigned long)field4_timestamp_s);

    ESP_LOGD(TAG, "POST body: %s", buf);
    return http_post_form("http://api.thingspeak.com/update", String(buf));
}
