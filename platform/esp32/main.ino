#include "../../common/app.h"

/* TODO: replace with real Lua dispatch once Lua is wired up for ESP32 */
static void noop_dispatch(uint32_t tag, app_t *app) { (void)tag; (void)app; }

static app_t s_app;
static uint32_t s_last_tick_ms = 0;

void setup()
{
    /* TODO: read RTC for wall-clock time */
    uint64_t now_unix_sec = 0; /* (uint64_t)rtc.now().unixtime() */

    app_init(&s_app, 0, now_unix_sec);
    s_app.dispatch_cb = noop_dispatch;

    uint32_t id = 0xDEADBEEFUL; /* TODO: random from esp_random() */
    app_spawn_character(&s_app, id);

    s_last_tick_ms = millis();
}

void loop()
{
    uint32_t now = millis();
    if (now - s_last_tick_ms >= AUTOTICK) {
        s_last_tick_ms = now;
        app_advance(&s_app, AUTOTICK, 0);
        /* TODO: render */
    }
    /* TODO: poll BLE, buttons */
}
