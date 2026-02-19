#ifndef CPU_H
#define CPU_H

#include <stdint.h>

void cpuid_get_vendor(char out[13]);
uint32_t cpuid_get_feature_ecx();
uint32_t cpuid_get_feature_edx();

#endif

