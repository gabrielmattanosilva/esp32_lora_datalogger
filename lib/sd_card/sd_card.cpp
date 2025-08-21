#include "sd_card.h"
#include <SPI.h>
#include <SD.h>
#include "pins.h"

#define SD_FLUSH_EVERY_N_LINES 8

static FS *g_fs = &SD;
static File g_file;
static uint8_t g_cs = 0xFF;
static bool g_sd_ok = false;
static int g_cur_ymd = -1;
static uint32_t g_lines_since_flush = 0;

/****************************** Funções privadas ******************************/

static inline bool tm_is_epoch0(const struct tm &tm)
{
    return (tm.tm_year + 1900) == 1970 && (tm.tm_mon + 1) == 1 && tm.tm_mday == 1;
}

static void make_filename_from_tm(const struct tm *tm, char *out, size_t outlen)
{
    snprintf(out, outlen, "/%04d%02d%02d_%02d%02d%02d.log",
             tm->tm_year + 1900,
             tm->tm_mon + 1,
             tm->tm_mday,
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec);
}

static void close_file()
{
    if (g_file)
    {
        g_file.flush();
        g_file.close();
    }
}

static void write_header_line()
{
    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    char hdr[128];
    snprintf(hdr, sizeof(hdr),
             "=== LOG START %04d-%02d-%02d %02d:%02d:%02d ===\n",
             tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
             tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);
    g_file.print(hdr);
    g_file.flush();
    g_lines_since_flush = 0;
}

static int current_ymd_from_tm(const struct tm *tm)
{
    return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
}

static unsigned long find_next_epoch0_seq()
{
    unsigned long max_seq = (unsigned long)0;
    bool found_any = false;

    File root = g_fs->open("/");

    if (!root)
    {
        return 0;
    }

    while (true)
    {
        File entry = root.openNextFile();

        if (!entry)
        {
            break;
        }

        if (!entry.isDirectory())
        {
            const char *nm = entry.name();
            const char *name = nm;

            if (name[0] == '/')
            {
                name++;
            }

            const char *prefix = "19700101_000000_";
            const size_t prefix_len = strlen(prefix);
            const size_t name_len = strlen(name);
            const char *suffix = ".log";
            const size_t suffix_len = 4;

            if (name_len > prefix_len + suffix_len &&
                strncmp(name, prefix, prefix_len) == 0 &&
                strcmp(name + name_len - suffix_len, suffix) == 0)
            {
                const char *p = name + prefix_len;
                bool ok_digits = true;
                unsigned long val = 0;

                while (*p && (p < name + name_len - suffix_len))
                {
                    if (!isdigit((unsigned char)*p))
                    {
                        ok_digits = false;
                        break;
                    }

                    unsigned digit = (unsigned)(*p - '0');
                    val = val * 10UL + digit;
                    p++;
                }

                if (ok_digits)
                {
                    if (!found_any || val > max_seq)
                    {
                        max_seq = val;
                        found_any = true;
                    }
                }
            }
        }
        entry.close();
    }

    root.close();
    return found_any ? (max_seq + 1UL) : 0UL;
}

static bool open_new_file_for_now()
{
    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    char fn[80];

    if (tm_is_epoch0(tm_local))
    {
        unsigned long seq = find_next_epoch0_seq();
        snprintf(fn, sizeof(fn), "/19700101_000000_%lu.log", seq);
        close_file();
        g_file = g_fs->open(fn, FILE_WRITE);

        if (!g_file)
        {
            return false;
        }

        g_cur_ymd = 19700101;
        write_header_line();
        return true;
    }

    make_filename_from_tm(&tm_local, fn, sizeof(fn));
    close_file();
    g_file = g_fs->open(fn, FILE_WRITE);

    if (!g_file)
    {
        return false;
    }

    g_cur_ymd = current_ymd_from_tm(&tm_local);
    write_header_line();
    return true;
}

static void ensure_file_for_today()
{
    if (!g_sd_ok)
    {
        return;
    }

    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_r(&now, &tm_local);

    if (!g_file)
    {
        (void)open_new_file_for_now();
        return;
    }

    if (tm_is_epoch0(tm_local))
    {
        return;
    }

    int ymd = current_ymd_from_tm(&tm_local);

    if (g_cur_ymd != ymd)
    {
        (void)open_new_file_for_now();
    }
}

/****************************** Funções públicas ******************************/

bool sdcard_begin()
{
    g_cs = SD_SPI_CS;
    pinMode(g_cs, OUTPUT);
    digitalWrite(g_cs, HIGH);
    g_sd_ok = SD.begin(g_cs, SPI, 20000000);

    if (!g_sd_ok)
    {
        return false;
    }

    g_sd_ok = open_new_file_for_now();
    return g_sd_ok;
}

void sdcard_tick_rotate()
{
    if (!g_sd_ok)
    {
        return;
    }

    ensure_file_for_today();
}

void sdcard_vprintf(const char *fmt, va_list ap)
{
    if (!g_sd_ok || !g_file)
    {
        return;
    }

    ensure_file_for_today();
    char line[512];
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(line, sizeof(line), fmt, ap2);
    va_end(ap2);

    if (n <= 0)
    {
        return;
    }

    size_t to_write = (n < (int)sizeof(line)) ? (size_t)n : (sizeof(line) - 1);
    g_file.write((const uint8_t *)line, to_write);

    if (++g_lines_since_flush >= SD_FLUSH_EVERY_N_LINES)
    {
        g_file.flush();
        g_lines_since_flush = 0;
    }
}

void sdcard_printf(const char *fmt, ...)
{
    if (!g_sd_ok || !g_file)
    {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    sdcard_vprintf(fmt, ap);
    va_end(ap);
}

void sdcard_flush()
{
    if (!g_sd_ok || !g_file)
    {
        return;
    }

    g_file.flush();
}

void sdcard_end()
{
    close_file();
    g_sd_ok = false;
}
