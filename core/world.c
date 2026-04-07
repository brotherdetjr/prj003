#include "world.h"

void world_init(world_t *w, uint64_t now_ts)
{
    w->now_ts        = now_ts;
    w->has_character = 0;
}

int world_spawn_character(world_t *w, uint32_t id)
{
    if (w->has_character)
        return -1;
    character_init(&w->character, id, w->now_ts);
    w->has_character = 1;
    return 0;
}

void world_tick(world_t *w)
{
    w->now_ts += WORLD_TICK_S;
    if (w->has_character)
        character_tick(&w->character);
    /* future: zodiac_tick(), encounter_tick(), ... */
}
