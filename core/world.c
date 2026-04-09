#include "world.h"
#include <stddef.h>

void world_init(world_t *w, uint64_t now_tick, uint64_t now_unix_sec)
{
    w->now_tick      = now_tick;
    w->now_unix_sec   = now_unix_sec;
    w->has_character = 0;
    scheduler_init(&w->scheduler);
}

int world_spawn_character(world_t *w, uint32_t id)
{
    if (w->has_character) return -1;
    character_init(&w->character, id, w->now_unix_sec, w->now_tick);
    w->has_character = 1;
    scheduler_add(&w->scheduler,
                  w->now_tick + CHARACTER_ENERGY_DRAIN_MS,
                  WORLD_EVENT_ENERGY_DRAIN);
    return 0;
}

int world_poof_character(world_t *w)
{
    if (!w->has_character) return -1;
    w->has_character = 0;
    scheduler_clear(&w->scheduler);
    return 0;
}

static void dispatch(world_t *w, const scheduled_event_t *ev)
{
    switch ((world_event_tag_t)ev->tag) {
        case WORLD_EVENT_ENERGY_DRAIN:
            if (w->has_character && w->character.energy > 0)
                w->character.energy--;
            /* reschedule for next drain even at energy=0 (no-op drain is fine) */
            scheduler_add(&w->scheduler,
                          ev->fire_at_ms + CHARACTER_ENERGY_DRAIN_MS,
                          WORLD_EVENT_ENERGY_DRAIN);
            break;
    }
}

advance_result_t world_advance(world_t *w, uint64_t ticks, int stop_on_event)
{
    advance_result_t r = { w->now_tick, 0, 0 };

    uint64_t target_tick;
    if (ticks == 0) {
        if (!stop_on_event) return r;          /* no-op */
        const scheduled_event_t *next = scheduler_peek(&w->scheduler);
        if (!next) return r;                   /* no event pending */
        target_tick = next->fire_at_ms;
    } else {
        target_tick = w->now_tick + ticks;
    }

    const scheduled_event_t *next;
    while ((next = scheduler_peek(&w->scheduler)) != NULL
           && next->fire_at_ms <= target_tick) {
        scheduled_event_t ev;
        scheduler_pop(&w->scheduler, &ev);
        w->now_tick = ev.fire_at_ms;
        dispatch(w, &ev);
        r.now_tick = w->now_tick;
        if (stop_on_event) {
            r.stopped_on_event = 1;
            r.event_tag        = ev.tag;
            return r;
        }
    }

    w->now_tick = target_tick;
    r.now_tick  = target_tick;
    return r;
}

void world_rebuild_scheduler(world_t *w)
{
    scheduler_clear(&w->scheduler);
    if (!w->has_character) return;
    /* Schedule next energy drain one full cycle from now */
    scheduler_add(&w->scheduler,
                  w->now_tick + CHARACTER_ENERGY_DRAIN_MS,
                  WORLD_EVENT_ENERGY_DRAIN);
}

const char *world_event_name(uint32_t tag)
{
    switch ((world_event_tag_t)tag) {
        case WORLD_EVENT_ENERGY_DRAIN: return "energy_drain";
        default:                       return "unknown";
    }
}
