#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "character.h"
#include "scheduler.h"

/*
 * Callback invoked by world_advance when a scheduled event fires.
 * The platform layer (server.c / main.ino) sets this to dispatch Lua events.
 * `tag`  — the scheduler tag that fired.
 * `ud`   — opaque pointer supplied at registration (app_t * on PC).
 */
typedef void (*world_dispatch_fn)(uint32_t tag, void *ud);

typedef struct {
    uint64_t          now_tick;     /* virtual clock in ms; advanced by world_advance */
    uint64_t          now_unix_sec;  /* wall-clock UTC seconds; set by platform / set_wall_clock */
    int               has_character;
    character_t       character;
    scheduler_t       scheduler;
    world_dispatch_fn dispatch_cb;  /* platform-supplied event handler; must be set before world_advance */
    void             *dispatch_ud;  /* userdata forwarded to dispatch_cb */
} world_t;

typedef struct {
    uint64_t now_tick;
    int      stopped_on_event;  /* non-zero if advance stopped at a fired event */
    uint32_t event_tag;         /* tag of the fired event; meaningful when stopped_on_event != 0 */
} advance_result_t;

/* Initialise an empty world. now_tick and now_unix_sec are set by the caller. */
void world_init(world_t *w, uint64_t now_tick, uint64_t now_unix_sec);

/*
 * Spawn a character. Uses w->now_tick as birth_tick and
 * w->now_unix_sec as birth_unix_sec.
 * Returns 0 on success, -1 if a character already exists.
 */
int world_spawn_character(world_t *w, uint32_t id);

/*
 * Remove the current character and clear all scheduled events.
 * Returns 0 on success, -1 if no character exists.
 */
int world_poof_character(world_t *w);

/*
 * Advance virtual time by `ticks` milliseconds, firing scheduled events in order.
 *
 * Behaviour matrix:
 *   ticks > 0, stop_on_event=0  — advance full ticks; fire all events in range.
 *   ticks > 0, stop_on_event=1  — advance up to ticks; stop after first event.
 *   ticks = 0, stop_on_event=1  — advance to the next scheduled event; no-op if none.
 *   ticks = 0, stop_on_event=0  — no-op.
 *
 * now_tick is updated to the event's fire_at_ms when stopping early,
 * or to now_tick + ticks when the full range is consumed.
 */
advance_result_t world_advance(world_t *w, uint64_t ticks, int stop_on_event);


#endif /* WORLD_H */
