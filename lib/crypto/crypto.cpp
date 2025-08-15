/**
 * @file crypto.cpp
 * @brief Implementação das funções de criptografia AES-128-CBC.
 */

#include "crypto.h"
#include <string.h>
#include <mbedtls/aes.h>
#include "serial_log.h"

static const char *TAG = "CRYPTO";
static uint8_t g_key[CRYPTO_KEY_SIZE];

/****************************** Funções privadas ******************************/

/**
 * @brief Remove padding PKCS#7 dos dados de entrada.
 * @param buf Dados com padding.
 * @param in_len Tamanho dos dados com padding.
 * @param out_len Ponteiro para armazenar o tamanho dos dados sem padding.
 * @return Verdadeiro se o padding foi removido com sucesso, falso caso contrário.
 */
static inline bool pkcs7_unpad(uint8_t *buf, size_t in_len, size_t *out_len)
{
    if (in_len == 0 || (in_len % CRYPTO_BLOCK_SIZE) != 0)
    {
        return false;
    }

    uint8_t pad = buf[in_len - 1];

    if (pad == 0 || pad > CRYPTO_BLOCK_SIZE)
    {
        return false;
    }

    for (size_t i = 0; i < pad; ++i)
    {
        if (buf[in_len - 1 - i] != pad)
        {
            return false;
        }
    }

    *out_len = in_len - pad;
    return true;
}

/****************************** Funções públicas ******************************/

/**
 * @brief Inicializa o módulo de criptografia com a chave fornecida.
 * @param key16 Ponteiro para a chave AES de 16 bytes.
 */
void crypto_init(const uint8_t *key16)
{
    memcpy(g_key, key16, CRYPTO_KEY_SIZE);
    LOGI(TAG, "AES-128-CBC inicializado");
}

/**
 * @brief Descriptografa dados usando AES-128-CBC.
 * @param in Dados criptografados.
 * @param in_len Tamanho dos dados criptografados.
 * @param iv Vetor de inicialização.
 * @param out Buffer para os dados descriptografados.
 * @param out_len Ponteiro para armazenar o tamanho dos dados descriptografados.
 * @return Verdadeiro se a descriptografia foi bem-sucedida, falso caso contrário.
 */
bool crypto_decrypt(const uint8_t *in, size_t in_len,
                    const uint8_t iv[CRYPTO_BLOCK_SIZE],
                    uint8_t *out, size_t *out_len)
{
    if ((in_len % CRYPTO_BLOCK_SIZE) != 0)
    {
        LOGE(TAG, "input length nao multiplo de 16: %u", (unsigned)in_len);
        return false;
    }

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    if (mbedtls_aes_setkey_dec(&ctx, g_key, 128) != 0)
    {
        LOGE(TAG, "setkey_dec falhou");
        mbedtls_aes_free(&ctx);
        return false;
    }

    uint8_t iv_copy[CRYPTO_BLOCK_SIZE];
    memcpy(iv_copy, iv, CRYPTO_BLOCK_SIZE);

    int32_t rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, in_len, iv_copy, in, out);
    mbedtls_aes_free(&ctx);

    if (rc != 0)
    {
        LOGE(TAG, "aes_crypt_cbc falhou (rc=%d)", rc);
        return false;
    }

    if (!pkcs7_unpad(out, in_len, out_len))
    {
        LOGW(TAG, "PKCS7 unpad invalido");
        return false;
    }

    LOGD(TAG, "decrypt OK (plain_len=%u)", (unsigned)*out_len);
    return true;
}