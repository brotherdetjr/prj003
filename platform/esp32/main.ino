#include "../common/app.h"

static world_t s_world;
static uint32_t s_last_tick_ms = 0;

void setup()
{
    /* TODO: read RTC for wall-clock time */
    uint64_t now_unix_sec = 0; /* (uint64_t)rtc.now().unixtime() */

    world_init(&s_world, 0, now_unix_sec);

    uint32_t id = 0xDEADBEEFUL; /* TODO: random from esp_random() */
    world_spawn_character(&s_world, id);

    s_last_tick_ms = millis();
}

void loop()
{
    uint32_t now = millis();
    if (now - s_last_tick_ms >= AUTOTICK_MS) {
        s_last_tick_ms = now;
        world_advance(&s_world, AUTOTICK, 0);
        /* TODO: render */
    }
    /* TODO: poll BLE, buttons */
}
