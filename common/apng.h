#ifndef GLOXIE_APNG_H
#define GLOXIE_APNG_H

#include <stdint.h>
#include <stdlib.h>

/*
 * Decode a PNG or APNG from memory.
 * Returns a malloc'd buffer of (*n) × (*w) × (*h) × 4 RGBA bytes, or NULL on
 * error. For a static PNG *n is set to 1. Caller must free() the result.
 */
uint8_t *apng_load(const uint8_t *data, size_t len, int *n, int *w, int *h);

#endif /* GLOXIE_APNG_H */
