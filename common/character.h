#ifndef CHARACTER_H
#define CHARACTER_H

#include <stdint.h>

typedef struct {
    uint32_t id;
    uint64_t birth_unix_sec; /* wall-clock UTC seconds at birth; used for zodiac */
    uint64_t birth_tick;    /* world now_tick at birth; virtual age = now_tick - birth_tick */
} character_t;

/*
 * Initialise a new character.
 * birth_unix_sec — real-world UTC seconds (from RTC / wall clock).
 * birth_tick — world->now_tick at the moment of spawning.
 */
void character_init(character_t *c, uint32_t id,
                    uint64_t birth_unix_sec, uint64_t birth_tick);

#endif /* CHARACTER_H */
