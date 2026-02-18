#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>

uint32_t djb2_hash(const char* str);
void xor_cipher(char* data, const char* key, int data_len);

#endif
