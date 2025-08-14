#include "logger.h"
#include <stdio.h>

void logger_init_default(void)
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    ESP_LOGI("MAIN", "Iniciando datalogger...");
}

void logger_set_level(const char* tag, esp_log_level_t level)
{
    if (!tag) return;
    esp_log_level_set(tag, level);
}

void logger_hexdump(const char* tag, esp_log_level_t level, const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return;

    // imprime 16 bytes por linha
    char line[3 * 16 + 1]; // "xx " * 16 + '\0'
    size_t idx = 0;

    while (idx < len)
    {
        int pos = 0;
        size_t base = idx;
        for (int i = 0; i < 16 && idx < len; ++i, ++idx)
        {
            pos += snprintf(&line[pos], sizeof(line) - pos, "%02X ", buf[idx]);
        }
        line[pos] = '\0';
        switch (level) {
            case ESP_LOG_ERROR: ESP_LOGE(tag, "%s", line); break;
            case ESP_LOG_WARN:  ESP_LOGW(tag, "%s", line); break;
            case ESP_LOG_INFO:  ESP_LOGI(tag, "%s", line); break;
            case ESP_LOG_DEBUG: ESP_LOGD(tag, "%s", line); break;
            default:            ESP_LOGV(tag, "%s", line); break;
        }
    }
}
