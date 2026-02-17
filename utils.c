#include "utils.h"

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) {
        --n;
        s1++;
        s2++;
    }
    if (n == 0) {
        return 0;
    } else {
        return *(const unsigned char*)s1 - *(const unsigned char*)s2;
    }
}

int strlen(const char* s) {
    int len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

void* memcpy(void* dest, const void* src, int n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memset(void* s, int c, int n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}
