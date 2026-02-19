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

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    int i = 0;
    
    // Handle negative numbers
    if (str[0] == '-') {
        sign = -1;
        i++;
    }
    
    // Process digits
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    
    return result * sign;
}

int hex_to_int(const char* str) {
    int result = 0;
    int i = 0;
    
    // Skip optional 0x prefix
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        i = 2;
    }
    
    // Process hex digits
    while (str[i]) {
        result *= 16;
        if (str[i] >= '0' && str[i] <= '9') {
            result += str[i] - '0';
        } else if (str[i] >= 'a' && str[i] <= 'f') {
            result += str[i] - 'a' + 10;
        } else if (str[i] >= 'A' && str[i] <= 'F') {
            result += str[i] - 'A' + 10;
        } else {
            break; // Invalid character
        }
        i++;
    }
    
    return result;
}
