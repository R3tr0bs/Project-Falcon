#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
void* memcpy(void* dest, const void* src, int n);
void* memset(void* s, int c, int n);

#endif