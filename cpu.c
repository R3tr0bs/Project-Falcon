#include "cpu.h"

void cpuid_get_vendor(char out[13]) {
    uint32_t eax, ebx, ecx, edx;
    eax = 0;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
    ((uint32_t*)out)[0] = ebx;
    ((uint32_t*)out)[1] = edx;
    ((uint32_t*)out)[2] = ecx;
    out[12] = '\0';
}

uint32_t cpuid_get_feature_ecx() {
    uint32_t eax=1, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
    return ecx;
}

uint32_t cpuid_get_feature_edx() {
    uint32_t eax=1, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
    return edx;
}

