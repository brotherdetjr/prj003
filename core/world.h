#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "character.h"

/* Game logic advances in 1-second ticks. */
#define WORLD_TICK_S 1U

typedef struct {
    uint64_t    now_ts;        /* current virtual time, UTC epoch seconds */
    uint8_t     has_character;
    character_t character;
} world_t;

/* Initialise an empty world with no character. */
void world_init(world_t *w, uint64_t now_ts);

/*
 * Create a character in the world.
 * birth_ts is taken from w->now_ts.
 * Returns 0 on success, -1 if a character already exists.
 */
int world_spawn_character(world_t *w, uint32_t id);

/* Advance the entire world by one tick (WORLD_TICK_S seconds). */
void world_tick(world_t *w);

#endif /* WORLD_H */
