#ifndef CHARACTER_H
#define CHARACTER_H

#include <stdint.h>

/* Opaque energy level: 0 = exhausted, 255 = fully rested */
typedef uint8_t energy_t;

/*
 * Energy drains by 1 unit every CHARACTER_ENERGY_DRAIN_MS milliseconds.
 * Default: full drain (255 → 0) in 24 hours.
 *   24h = 86,400,000 ms / 255 ≈ 339,000 ms per unit
 */
#define CHARACTER_ENERGY_DRAIN_MS 339000ULL

typedef struct {
    uint32_t id;
    uint64_t birth_unix_sec; /* wall-clock UTC seconds at birth; used for zodiac */
    uint64_t birth_tick;    /* world now_tick at birth; virtual age = now_tick - birth_tick */
    energy_t energy;
} character_t;

/*
 * Initialise a new character.
 * birth_unix_sec — real-world UTC seconds (from RTC / wall clock).
 * birth_tick — world->now_tick at the moment of spawning.
 */
void character_init(character_t *c, uint32_t id,
                    uint64_t birth_unix_sec, uint64_t birth_tick);

#endif /* CHARACTER_H */
