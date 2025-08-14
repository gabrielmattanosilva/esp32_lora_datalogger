/**
 * @file crypto.h
 * @brief Cabeçalho para as funções de criptografia AES-128-CBC.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CRYPTO_KEY_SIZE 16
#define CRYPTO_BLOCK_SIZE 16

void crypto_init(const uint8_t *key16);
bool crypto_decrypt(const uint8_t *in, size_t in_len,
                    const uint8_t iv[CRYPTO_BLOCK_SIZE],
                    uint8_t *out, size_t *out_len);

#endif /* CRYPTO_H */
