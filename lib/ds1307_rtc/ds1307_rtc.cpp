#include "ds1307_rtc.h"
#include <Wire.h>
#include <RTClib.h>
#include "pins.h"
#include "logger.h"

static const char *TAG = "DS1307";
static RTC_DS1307 g_rtc;
static bool g_wire_started = false;
static bool g_rtc_ready = false;

/****************************** Funções privadas ******************************/

static bool datetime_is_reasonable(const DateTime &dt)
{
    const int y = dt.year();
    return (y >= 2000) && (y <= 2099);
}

bool ds1307_rtc_begin(void)
{
    if (!g_wire_started)
    {
        Wire.begin(I2C_SDA, I2C_SCL);
        Wire.setClock(100000UL);
        g_wire_started = true;
    }

    if (!g_rtc.begin(&Wire))
    {
        LOG(TAG, "DS1307 nao respondeu no I2C");
        g_rtc_ready = false;
        return false;
    }

    g_rtc_ready = true;

    if (!g_rtc.isrunning())
    {
        LOG(TAG, "DS1307 presente, mas relogio PARADO (isrunning()=false)");
    }
    else
    {
        LOG(TAG, "DS1307 OK e rodando");
    }

    return true;
}

/****************************** Funções públicas ******************************/

bool ds1307_rtc_sync_at_boot(void)
{
    if (!g_rtc_ready && !ds1307_rtc_begin())
    {
        return false;
    }

    if (!g_rtc.isrunning())
    {
        LOG(TAG, "Relogio PARADO, nao sincronizando o RTC interno.");
        return false;
    }

    DateTime now = g_rtc.now();

    if (!datetime_is_reasonable(now))
    {
        LOG(TAG, "Data/hora do DS1307 invalida (ano=%d), nao sincronizando.", now.year());
        return false;
    }

    struct timeval tv;
    tv.tv_sec = (time_t)now.unixtime();
    tv.tv_usec = 0;

    if (settimeofday(&tv, nullptr) != 0)
    {
        LOG(TAG, "settimeofday() falhou");
        return false;
    }

    struct tm tm_local;
    time_t t = tv.tv_sec;
    localtime_r(&t, &tm_local);
    LOG(TAG, "RTC interno sincronizado: %04d/%02d/%02d %02d:%02d:%02d",
        tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
        tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);
    return true;
}
