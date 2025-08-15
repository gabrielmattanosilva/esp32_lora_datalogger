/**
 * @file serial_log.h
 * @brief Macros de logging via Serial, no estilo esp_log.
 */
#pragma once
#include <Arduino.h>

/**
 * @brief Log de informação.
 * @param TAG Nome da TAG do módulo (ex.: "MAIN").
 * @param fmt Formato printf-like.
 */
#define LOGI(TAG, fmt, ...)                                                                  \
    do                                                                                       \
    {                                                                                        \
        Serial.printf("I (%lu) %s: " fmt "\n", (unsigned long)millis(), TAG, ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Log de aviso.
 * @param TAG Nome da TAG do módulo.
 * @param fmt Formato printf-like.
 */
#define LOGW(TAG, fmt, ...)                                                                  \
    do                                                                                       \
    {                                                                                        \
        Serial.printf("W (%lu) %s: " fmt "\n", (unsigned long)millis(), TAG, ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Log de erro.
 * @param TAG Nome da TAG do módulo.
 * @param fmt Formato printf-like.
 */
#define LOGE(TAG, fmt, ...)                                                                  \
    do                                                                                       \
    {                                                                                        \
        Serial.printf("E (%lu) %s: " fmt "\n", (unsigned long)millis(), TAG, ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Log de depuração.
 * @param TAG Nome da TAG do módulo.
 * @param fmt Formato printf-like.
 */
#define LOGD(TAG, fmt, ...)                                                                  \
    do                                                                                       \
    {                                                                                        \
        Serial.printf("D (%lu) %s: " fmt "\n", (unsigned long)millis(), TAG, ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Log verbose.
 * @param TAG Nome da TAG do módulo.
 * @param fmt Formato printf-like.
 */
#define LOGV(TAG, fmt, ...)                                                                  \
    do                                                                                       \
    {                                                                                        \
        Serial.printf("V (%lu) %s: " fmt "\n", (unsigned long)millis(), TAG, ##__VA_ARGS__); \
    } while (0)

/**
 * @brief Imprime um hexdump de um buffer em linhas de 16 bytes.
 * @param TAG TAG do módulo.
 * @param level Nível ('I','W','E','D','V').
 * @param buf Ponteiro para o buffer.
 * @param len Tamanho do buffer em bytes.
 */
static inline void LOG_HEXDUMP(const char *TAG, char level, const uint8_t *buf, size_t len)
{
    if (!buf || len == 0)
    {
        return;
    }

    char line[3 * 16 + 1];
    size_t idx = 0;

    while (idx < len)
    {
        int pos = 0;

        for (int i = 0; i < 16 && idx < len; ++i, ++idx)
        {
            pos += snprintf(&line[pos], sizeof(line) - pos, "%02X ", buf[idx]);
        }

        line[pos] = '\0';

        switch (level)
        {
        case 'I':
            LOGI(TAG, "%s", line);
            break;
        case 'W':
            LOGW(TAG, "%s", line);
            break;
        case 'E':
            LOGE(TAG, "%s", line);
            break;
        case 'V':
            LOGV(TAG, "%s", line);
            break;
        default:
            LOGD(TAG, "%s", line);
            break;
        }
    }
}
