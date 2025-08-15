/**
 * @file thingspeak_client.cpp
 * @brief Implementação do cliente ThingSpeak (HTTP).
 */

#include "thingspeak_client.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "wifi_manager.h"
#include "serial_log.h"

static const char *TAG = "TS";

/****************************** Funções privadas ******************************/

/**
 * @brief Executa um POST simples (form-urlencoded).
 * @param url URL de destino.
 * @param body Corpo do POST no formato "k=v&...".
 * @return true em caso de HTTP 200 com payload; false caso contrário.
 */
static bool http_post_form(const String &url, const String &body)
{
    if (!wifi_is_connected())
    {
        return false;
    }

    HTTPClient http;
    WiFiClient client;

    if (!http.begin(client, url))
    {
        LOGE(TAG, "http.begin() falhou");
        return false;
    }

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int32_t code = http.POST(body);
    String payload = http.getString();
    http.end();

    LOGI(TAG, "HTTP %d, payload_len=%d", code, payload.length());
    return (code == 200) && payload.length() > 0;
}

/****************************** Funções públicas ******************************/

/**
 * @brief Envia 4 campos para o ThingSpeak.
 * @param api_key Chave de escrita do canal.
 * @param field1_irradiance_Wm2 Irradiância (W/m^2). Use -1.0 em caso de erro.
 * @param field2_batt_V Tensão da bateria (V).
 * @param field3_temp_C Temperatura interna (°C).
 * @param field4_timestamp_s Timestamp em segundos.
 * @return true se resposta HTTP 200 e payload não vazio; false caso contrário.
 */
bool thingspeak_update(const char *api_key,
                       float field1_irradiance_Wm2,
                       float field2_batt_V,
                       float field3_temp_C,
                       uint32_t field4_timestamp_s)
{
    if (!wifi_is_connected())
    {
        LOGW(TAG, "sem Wi-Fi. Nao enviado.");
        return false;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "api_key=%s&field1=%.1f&field2=%.3f&field3=%.1f&field4=%lu",
             api_key, field1_irradiance_Wm2, field2_batt_V, field3_temp_C,
             (uint32_t)field4_timestamp_s);
    return http_post_form("http://api.thingspeak.com/update", String(buf));
}
