/**
 * @file wifi_manager.cpp
 * @brief Implementação do gerenciador de Wi-Fi.
 */

#include "wifi_manager.h"
#include <WiFi.h>
#include "serial_log.h"

/* Parâmetros de retry */
#define BACKOFF_MIN_S 3000
#define BACKOFF_MAX_S 300000
#define CONNECT_GUARD_MS 12000

/* Estados */
static String g_ssid, g_pass;
static bool g_began = false;
static uint32_t g_next_try_ms = 0;
static uint32_t g_backoff_ms = 0;
static wl_status_t g_prev_status = (wl_status_t)0xFF;

static const char *TAG = "WIFI";
static const char *wl_status_to_str(wl_status_t s)
{
    switch (s)
    {
    case WL_IDLE_STATUS:
        return "WL_IDLE_STATUS(0)";
    case WL_NO_SSID_AVAIL:
        return "WL_NO_SSID_AVAIL(1)";
    case WL_SCAN_COMPLETED:
        return "WL_SCAN_COMPLETED(2)";
    case WL_CONNECTED:
        return "WL_CONNECTED(3)";
    case WL_CONNECT_FAILED:
        return "WL_CONNECT_FAILED(4)";
    case WL_CONNECTION_LOST:
        return "WL_CONNECTION_LOST(5)";
    case WL_DISCONNECTED:
        return "WL_DISCONNECTED(6)";
    case WL_NO_SHIELD:
        return "WL_NO_SHIELD(255)";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Inicializa o gerenciador de Wi-Fi.
 * @param ssid SSID da rede.
 * @param pass Senha da rede.
 */
void wifi_begin(const char *ssid, const char *pass)
{
    g_ssid = ssid ? ssid : "";
    g_pass = pass ? pass : "";
    WiFi.mode(WIFI_STA);
    g_began = false;
    g_backoff_ms = 0;
    g_next_try_ms = 0;
    g_prev_status = (wl_status_t)0xFF;
    randomSeed((uint32_t)esp_random());
    LOGI(TAG, "init (SSID=\"%s\")", g_ssid.c_str());
}

/**
 * @brief Tenta conectar-se a rede Wi-Fi.
 */
static void wifi_start_connect(void)
{
    if (g_ssid.isEmpty())
    {
        return;
    }

    if (!g_began)
    {
        LOGI(TAG, "begin() tentando conectar a \"%s\"...", g_ssid.c_str());
        WiFi.begin(g_ssid.c_str(), g_pass.c_str());
        g_began = true;
    }
    else
    {
        LOGI(TAG, "reconnect() tentando reconectar...");
        WiFi.reconnect();
    }
}

/**
 * @brief Informa se está conectado ao AP.
 * @return true se WL_CONNECTED; false caso contrário.
 */
bool wifi_is_connected(void)
{
    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Retorna IP local em string estática (ex.: "192.168.0.10").
 * @return Ponteiro para string estática.
 */
const char *wifi_ip_str(void)
{
    static char ipbuf[20];
    IPAddress ip = WiFi.localIP();
    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return ipbuf;
}

/**
 * @brief Retorna RSSI (dBm) quando conectado.
 * @return RSSI ou 0 se desconectado.
 */
int32_t wifi_rssi(void)
{
    return wifi_is_connected() ? WiFi.RSSI() : 0;
}

/**
 * @brief Força uma nova tentativa imediata (zera o backoff).
 */
void wifi_force_reconnect(void)
{
    LOGW(TAG, "force_reconnect()");
    g_began = false;
    g_backoff_ms = 0;
    g_next_try_ms = 0;
}

/**
 * @brief Avança a máquina de estados de reconexão (chamar a cada loop).
 * @param now_ms Tempo corrente (millis()).
 */
void wifi_tick(uint32_t now_ms)
{
    wl_status_t cur = WiFi.status();

    if (cur != g_prev_status)
    {
        if (cur == WL_CONNECTED)
        {
            LOGI(TAG, "CONECTADO  IP=%s  RSSI=%d dBm", wifi_ip_str(), (int)WiFi.RSSI());
            g_backoff_ms = BACKOFF_MIN_S;
            g_next_try_ms = now_ms + CONNECT_GUARD_MS;
        }
        else
        {
            LOGW(TAG, "DESCONECTADO (%s)", wl_status_to_str(cur));
            if (g_backoff_ms == 0)
            {
                g_backoff_ms = BACKOFF_MIN_S;
            }
        }

        g_prev_status = cur;
    }

    if (cur == WL_CONNECTED)
    {
        if (g_backoff_ms == 0)
        {
            g_backoff_ms = BACKOFF_MIN_S;
        }

        if (g_next_try_ms < now_ms + CONNECT_GUARD_MS)
        {
            g_next_try_ms = now_ms + CONNECT_GUARD_MS;
        }

        return;
    }

    if (g_backoff_ms == 0)
    {
        g_backoff_ms = BACKOFF_MIN_S;
    }

    if ((int32_t)(now_ms - g_next_try_ms) >= 0)
    {
        wifi_start_connect();
        g_next_try_ms = now_ms + CONNECT_GUARD_MS;
        g_backoff_ms = (g_backoff_ms < BACKOFF_MAX_S) ? (g_backoff_ms * 2) : BACKOFF_MAX_S;
        uint32_t jitter = g_backoff_ms / 10U;
        g_next_try_ms += (jitter ? random(0, jitter) : 0);
        LOGD(TAG, "proxima janela em ~%u ms", (unsigned)(g_next_try_ms - now_ms));
    }
}
