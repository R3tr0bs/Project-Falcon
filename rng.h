#ifndef RNG_H
#define RNG_H

#include <stdint.h>

void rng_init(uint32_t seed);
uint32_t rand32();
void rand_bytes(uint8_t* out, uint32_t len);

#endif

