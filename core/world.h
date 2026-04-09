#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "character.h"
#include "scheduler.h"

/* Tags used to schedule world events. Interpreted by world.c dispatch. */
typedef enum {
    WORLD_EVENT_ENERGY_DRAIN = 0
} world_event_tag_t;

/* Human-readable name for a world event tag (for JSON responses). */
const char *world_event_name(uint32_t tag);

typedef struct {
    uint64_t      now_tick;     /* virtual clock in ms; advanced by world_advance */
    uint64_t      now_unix_ms;  /* wall-clock UTC ms; set by platform / set_wall_clock */
    int           has_character;
    character_t   character;
    scheduler_t   scheduler;
} world_t;

typedef struct {
    uint64_t now_tick;
    int      stopped_on_event;  /* non-zero if advance stopped at a fired event */
    uint32_t event_tag;         /* meaningful only when stopped_on_event != 0 */
} advance_result_t;

/* Initialise an empty world. now_tick and now_unix_ms are set by the caller. */
void world_init(world_t *w, uint64_t now_tick, uint64_t now_unix_ms);

/*
 * Spawn a character. Uses w->now_tick as birth_tick and
 * w->now_unix_ms as birth_unix_ms.
 * Returns 0 on success, -1 if a character already exists.
 */
int world_spawn_character(world_t *w, uint32_t id);

/*
 * Remove the current character and clear all scheduled events.
 * Returns 0 on success, -1 if no character exists.
 */
int world_poof_character(world_t *w);

/*
 * Advance virtual time to target_tick, firing scheduled events in order.
 * If stop_on_event != 0, stops immediately after the first event fires.
 * now_tick is updated to the event's fire_at_ms when stopping early,
 * or to target_tick when the full range is consumed.
 */
advance_result_t world_advance(world_t *w, uint64_t target_tick, int stop_on_event);

/*
 * Rebuild the scheduler from current world state.
 * Call after restoring state via set_state so derived events are re-queued.
 */
void world_rebuild_scheduler(world_t *w);

#endif /* WORLD_H */
