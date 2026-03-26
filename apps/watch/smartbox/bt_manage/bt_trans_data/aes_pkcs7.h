/*
 *  Copyright 2014-2022 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef _AES_PKCS7_H_
#define _AES_PKCS7_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AES128_KEY_BITS 128
#define AES192_KEY_BITS 192
#define AES256_KEY_BITS 256

#define AES128_KEY_SIZE (AES128_KEY_BITS / 8)
#define AES192_KEY_SIZE (AES192_KEY_BITS / 8)
#define AES256_KEY_SIZE (AES256_KEY_BITS / 8)

#define AES_BLOCK_SIZE 16

#define AES128_ROUNDS 10
#define AES192_ROUNDS 12
#define AES256_ROUNDS 14
#define AES_MAX_ROUNDS AES256_ROUNDS

typedef struct {
  uint32_t rk[4 * (AES_MAX_ROUNDS + 1)];
  size_t rounds;
} AES_KEY;

int aes_set_encrypt_key(AES_KEY *key, const uint8_t *raw_key,
                        size_t raw_key_len);
int aes_set_decrypt_key(AES_KEY *key, const uint8_t *raw_key,
                        size_t raw_key_len);
void aes_encrypt(const AES_KEY *key, const uint8_t in[AES_BLOCK_SIZE],
                 uint8_t out[AES_BLOCK_SIZE]);
void aes_decrypt(const AES_KEY *key, const uint8_t in[AES_BLOCK_SIZE],
                 uint8_t out[AES_BLOCK_SIZE]);

uint8_t aes_decrypt_pkcs(const AES_KEY *aes_key, uint8_t *in, uint16_t in_len,
                         uint8_t *out, uint16_t *out_len);
uint16_t aes_encrypt_pkcs(const AES_KEY *aes_key, uint8_t *in, uint16_t in_len,
                          uint8_t *out, uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif // _AES_PKCS7_H_
