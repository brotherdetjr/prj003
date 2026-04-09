#include "character.h"

void character_init(character_t *c, uint32_t id,
                    uint64_t birth_unix_sec, uint64_t birth_tick)
{
    c->id             = id;
    c->birth_unix_sec  = birth_unix_sec;
    c->birth_tick  = birth_tick;
    c->energy         = 255;
}
