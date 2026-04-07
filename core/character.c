#include "character.h"

void character_init(character_t *c, uint32_t id, uint64_t birth_ts)
{
    c->id        = id;
    c->birth_ts  = birth_ts;
    c->energy    = 255;
    c->_drain_acc = 0;
}

void character_tick(character_t *c)
{
    if (c->energy == 0)
        return;

    if (++c->_drain_acc >= CHARACTER_ENERGY_DRAIN_S) {
        c->_drain_acc = 0;
        c->energy--;
    }
}
