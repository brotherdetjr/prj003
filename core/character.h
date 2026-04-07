#ifndef CHARACTER_H
#define CHARACTER_H

#include <stdint.h>

/* Opaque energy level: 0 = exhausted, 255 = fully rested */
typedef uint8_t energy_t;

/*
 * Energy drains by 1 unit every CHARACTER_ENERGY_DRAIN_S ticks (seconds).
 * Default: full drain (255 → 0) in 24 hours.
 *   24h = 86,400 s / 255 ≈ 339 s per unit
 */
#define CHARACTER_ENERGY_DRAIN_S 339U

typedef struct {
    uint32_t id;          /* randomly generated at birth */
    uint64_t birth_ts;    /* UTC Unix epoch, seconds */
    energy_t energy;
    uint16_t _drain_acc;  /* ticks accumulated toward next energy drop */
} character_t;

/*
 * Initialise a new character.
 * id and birth_ts are supplied by the platform (RNG + RTC).
 * Energy starts at full.
 */
void character_init(character_t *c, uint32_t id, uint64_t birth_ts);

/*
 * Advance character state by one tick (WORLD_TICK_S seconds).
 * Call this from world_tick(); do not call directly.
 */
void character_tick(character_t *c);

#endif /* CHARACTER_H */
