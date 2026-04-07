#include "../../core/world.h"

static world_t s_world;

void setup()
{
    /* TODO: seed RNG, read RTC */
    uint64_t now_ts = 0; /* (uint64_t)(rtc.now().unixtime()) */
    world_init(&s_world, now_ts);

    uint32_t id = 0xDEADBEEFUL; /* TODO: random from esp_random() */
    world_spawn_character(&s_world, id);
}

void loop()
{
    world_tick(&s_world);
    /* TODO: render */
    delay(WORLD_TICK_S * 1000U);
}
