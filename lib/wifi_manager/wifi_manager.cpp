#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_log.h>

static const char* TAG = "WIFI";

// ---- Estado interno ----
static String   g_ssid, g_pass;
static bool     g_began          = false;
static uint32_t g_next_try_ms    = 0;
static uint32_t g_backoff_ms     = 0;
static wl_status_t g_prev_status = WL_NO_SHIELD;

// Parâmetros do retry
static const uint32_t BACKOFF_MIN      = 3000;     // 3 s
static const uint32_t BACKOFF_MAX      = 300000;   // 5 min
static const uint32_t CONNECT_GUARD_MS = 12000;    // janela p/ associar/obter IP

void wifi_init(const char* ssid, const char* pass)
{
    g_ssid = ssid ? ssid : "";
    g_pass = pass ? pass : "";

    WiFi.mode(WIFI_STA);
    g_began       = false;
    g_backoff_ms  = 0;
    g_next_try_ms = 0;
    g_prev_status = WiFi.status();

    randomSeed((uint32_t)esp_random());

    ESP_LOGI(TAG, "init (SSID=\"%s\")", g_ssid.c_str());
}

static void wifi_start_connect(void)
{
    if (g_ssid.isEmpty()) return;

    if (!g_began) {
        ESP_LOGI(TAG, "begin() tentando conectar...");
        WiFi.begin(g_ssid.c_str(), g_pass.c_str());
        g_began = true;
    } else {
        ESP_LOGI(TAG, "reconnect() tentando reconectar...");
        WiFi.reconnect();
    }
}

bool wifi_is_connected(void)
{
    return (WiFi.status() == WL_CONNECTED);
}

const char* wifi_ip_str(void)
{
    static char ipbuf[20];
    IPAddress ip = WiFi.localIP();
    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return ipbuf;
}

int32_t wifi_rssi(void)
{
    return wifi_is_connected() ? WiFi.RSSI() : 0;
}

void wifi_force_reconnect(void)
{
    ESP_LOGW(TAG, "force_reconnect()");
    g_began       = false;
    g_backoff_ms  = 0;
    g_next_try_ms = 0;
}

void wifi_tick(uint32_t now_ms)
{
    wl_status_t cur = WiFi.status();

    // Logs de transição
    if (cur != g_prev_status) {
        switch (cur) {
            case WL_CONNECTED:
                ESP_LOGI(TAG, "CONECTADO  IP=%s  RSSI=%d dBm", wifi_ip_str(), (int)WiFi.RSSI());
                g_backoff_ms  = BACKOFF_MIN;
                g_next_try_ms = now_ms + CONNECT_GUARD_MS;
                break;

            case WL_DISCONNECTED:
            case WL_CONNECTION_LOST:
            case WL_NO_SSID_AVAIL:
            case WL_CONNECT_FAILED:
            default:
                ESP_LOGW(TAG, "DESCONECTADO (status=%d)", (int)cur);
                if (g_backoff_ms == 0) g_backoff_ms = BACKOFF_MIN;
                break;
        }
        g_prev_status = cur;
    }

    if (cur == WL_CONNECTED) {
        if (g_backoff_ms == 0) g_backoff_ms = BACKOFF_MIN;
        if (g_next_try_ms < now_ms + CONNECT_GUARD_MS)
            g_next_try_ms = now_ms + CONNECT_GUARD_MS;
        return;
    }

    if (g_backoff_ms == 0) g_backoff_ms = BACKOFF_MIN;

    if ((int32_t)(now_ms - g_next_try_ms) >= 0) {
        wifi_start_connect();

        g_next_try_ms = now_ms + CONNECT_GUARD_MS;

        g_backoff_ms = (g_backoff_ms < BACKOFF_MAX) ? (g_backoff_ms * 2) : BACKOFF_MAX;
        uint32_t jitter = g_backoff_ms / 10U;
        g_next_try_ms += (jitter ? random(0, jitter) : 0);

        ESP_LOGD(TAG, "agenda nova tentativa em ~%u ms", (unsigned)(g_next_try_ms - now_ms));
    }
}
