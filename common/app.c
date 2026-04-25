#include "app.h"

void app_init(app_t *app, uint64_t now_tick, uint64_t now_unix_sec)
{
    app->now_tick = now_tick;
    app->now_unix_sec = now_unix_sec;
    app->has_character = 0;
    scheduler_init(&app->scheduler);
}

int app_spawn_character(app_t *app, uint32_t id)
{
    if (app->has_character) return -1;
    character_init(&app->character, id, app->now_unix_sec, app->now_tick);
    app->has_character = 1;
    return 0;
}

int app_poof_character(app_t *app)
{
    if (!app->has_character) return -1;
    app->has_character = 0;
    scheduler_clear(&app->scheduler);
    return 0;
}

advance_result_t app_advance(app_t *app, uint64_t ticks, int stop_on_event)
{
    advance_result_t r = {app->now_tick, 0, 0};

    uint64_t target_tick;
    if (ticks == 0) {
        if (!stop_on_event) return r;
        const scheduled_event_t *next = scheduler_peek(&app->scheduler);
        if (!next) return r;
        target_tick = next->fire_at_ms;
    } else {
        target_tick = app->now_tick + ticks;
    }

    const scheduled_event_t *next;
    while ((next = scheduler_peek(&app->scheduler)) != NULL && next->fire_at_ms <= target_tick) {
        scheduled_event_t ev;
        scheduler_pop(&app->scheduler, &ev);
        app->now_tick = ev.fire_at_ms;
        app->dispatch_cb(ev.tag, app);
        r.now_tick = app->now_tick;
        if (stop_on_event) {
            r.stopped_on_event = 1;
            r.event_tag = ev.tag;
            return r;
        }
    }

    app->now_tick = target_tick;
    r.now_tick = target_tick;
    return r;
}
