#include "logger.h"
#include <Arduino.h>
#include "sd_card.h"

static bool g_log_ready = false;
static char s_line[256];

/****************************** Funções privadas ******************************/

static void format_timestamp(char *out, size_t outlen)
{
    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    uint32_t ms = (uint32_t)(millis() % 1000U);
    snprintf(out, outlen, "%04d/%02d/%02d %02d:%02d:%02d.%03u",
             tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
             tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec, ms);
}

/****************************** Funções públicas ******************************/

void logger_init_epoch0()
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

void logger_begin()
{
    if (!Serial)
    {
        Serial.begin(115200);
        uint32_t t0 = millis();
        while (!Serial && (millis() - t0) < 800)
        {
        }
    }

    g_log_ready = true;
    char ts[40];
    format_timestamp(ts, sizeof(ts));

    if (Serial)
    {
        Serial.printf("%s [LOGGER] pronto\n", ts);
    }

    sdcard_printf("%s [LOGGER] pronto\n", ts);
}

void logger_log(const char *tag, const char *fmt, ...)
{
    if (!g_log_ready)
    {
        return;
    }

    const char *t = tag ? tag : "LOG";
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_line, sizeof(s_line), fmt, ap);
    va_end(ap);
    char ts[40];
    format_timestamp(ts, sizeof(ts));

    if (Serial)
    {
        Serial.print(ts);
        Serial.print(" [");
        Serial.print(t);
        Serial.print("] ");
        Serial.println(s_line);
    }

    sdcard_printf("%s [%s] %s\n", ts, t, s_line);
}

void logger_hexdump(const char *tag, const uint8_t *buf, size_t len)
{
    if (!buf || len == 0)
    {
        logger_log(tag, "(hexdump vazio)");
        return;
    }

    char ts[40];
    format_timestamp(ts, sizeof(ts));
    const char *t = tag ? tag : "LOG";

    if (Serial)
    {
        Serial.printf("%s [%s] HEXDUMP (%u bytes):\n", ts, t, (unsigned)len);
    }

    sdcard_printf("%s [%s] HEXDUMP (%u bytes):\n", ts, t, (unsigned)len);

    for (size_t off = 0; off < len; off += 16)
    {
        uint32_t pos = 0;
        char line[3 * 16 + 1];

        for (size_t i = 0; i < 16 && (off + i) < len; ++i)
        {
            pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%02X ", buf[off + i]);
        }

        line[(pos < (uint32_t)sizeof(line)) ? pos : (uint32_t)sizeof(line) - 1] = '\0';

        if (Serial)
        {
            Serial.printf("%s [%s] %s\n", ts, t, line);
        }

        sdcard_printf("%s [%s] %s\n", ts, t, line);
    }
}
