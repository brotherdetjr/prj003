#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "../../core/character.h"
#include "../../core/scheduler.h"
#include "../../vendor/mongoose/mongoose.h"
#include "../../vendor/lua/lua.h"

#define AUTOTICK 100U   /* virtual ms per timer tick; timer fires 10×/sec for ~10 FPS */

#define LUA_MAX_EVENTS  64U

typedef struct { char name[32]; } lua_event_t;

typedef struct {
    uint64_t now_tick;
    int      stopped_on_event;
    uint32_t event_tag;
} advance_result_t;

typedef struct app_t {
    /* world state — serialisable, drives game logic */
    uint64_t          now_tick;       /* virtual clock in ms; advanced by world_advance */
    uint64_t          now_unix_sec;   /* wall-clock UTC seconds; set by platform / set_wall_clock */
    int               has_character;
    character_t       character;
    scheduler_t       scheduler;
    void            (*dispatch_cb)(uint32_t tag, struct app_t *app);  /* must be set before world_advance */

    /* runtime state — not serialised */
    char          instance_id[9];
    uint32_t      instance_id_raw;
    int           autotick;
    struct mg_mgr mgr;
    lua_State    *L;
    lua_event_t   lua_events[LUA_MAX_EVENTS];
    char          last_event_name[32];
    char          script_path[512];
} app_t;

/* Initialise an empty world. now_tick and now_unix_sec are set by the caller. */
void world_init(app_t *app, uint64_t now_tick, uint64_t now_unix_sec);

/*
 * Spawn a character. Uses app->now_tick as birth_tick and
 * app->now_unix_sec as birth_unix_sec.
 * Returns 0 on success, -1 if a character already exists.
 */
int world_spawn_character(app_t *app, uint32_t id);

/*
 * Remove the current character and clear all scheduled events.
 * Returns 0 on success, -1 if no character exists.
 */
int world_poof_character(app_t *app);

/*
 * Advance virtual time by `ticks` milliseconds, firing scheduled events in order.
 *
 * Behaviour matrix:
 *   ticks > 0, stop_on_event=0  — advance full ticks; fire all events in range.
 *   ticks > 0, stop_on_event=1  — advance up to ticks; stop after first event.
 *   ticks = 0, stop_on_event=1  — advance to the next scheduled event; no-op if none.
 *   ticks = 0, stop_on_event=0  — no-op.
 */
advance_result_t world_advance(app_t *app, uint64_t ticks, int stop_on_event);

#endif /* APP_H */
