#include "rng.h"

static uint32_t state = 1;

void rng_init(uint32_t seed) {
    if (seed == 0) seed = 1;
    state = seed;
}

uint32_t rand32() {
    state = state * 1664525u + 1013904223u;
    return state;
}

void rand_bytes(uint8_t* out, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if ((i & 3) == 0) {
            state = state * 1664525u + 1013904223u;
        }
        out[i] = (uint8_t)(state >> ((i & 3) * 8));
    }
}

