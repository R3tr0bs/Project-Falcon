#include "crypto.h"
#include "utils.h"

uint32_t djb2_hash(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

void xor_cipher(char* data, const char* key, int data_len) {
    int key_len = strlen(key);
    if (key_len == 0) return;
    
    for (int i = 0; i < data_len; i++) {
        data[i] ^= key[i % key_len];
    }
}
