#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "lua_bind.h"

cJSON *app_state_to_json(app_t *app)
{
    cJSON *root = cJSON_CreateObject();

    /* ro: read-only snapshot */
    cJSON *ro = cJSON_CreateObject();
    cJSON_AddStringToObject(ro, "instance_id", app->instance_id);
    cJSON_AddNumberToObject(ro, "now_tick", (double)app->now_tick);
    cJSON_AddNumberToObject(ro, "now_unix_sec", (double)app->now_unix_sec);

    if (app->has_character) {
        const character_t *ch = &app->character;
        char id_str[9];
        snprintf(id_str, sizeof(id_str), "%08X", ch->id);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "id", id_str);
        cJSON_AddNumberToObject(c, "birth_unix_sec", (double)ch->birth_unix_sec);
        cJSON_AddNumberToObject(c, "birth_tick", (double)ch->birth_tick);
        cJSON_AddItemToObject(ro, "character", c);
    } else {
        cJSON_AddNullToObject(ro, "character");
    }
    cJSON_AddItemToObject(root, "ro", ro);

    /* rw: read-write scripted state */
    cJSON_AddItemToObject(root, "rw", lua_bind_rw_to_cjson(app));

    /* scheduler: pending Lua events */
    cJSON *sched = cJSON_CreateArray();
    for (int i = 0; i < app->scheduler.count; i++) {
        const scheduled_event_t *ev = &app->scheduler.heap[i];
        uint32_t slot = ev->tag;
        if (slot >= LUA_MAX_EVENTS || app->lua_events[slot].name[0] == '\0') continue;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "fire_at_ms", (double)ev->fire_at_ms);
        cJSON_AddStringToObject(entry, "event", app->lua_events[slot].name);
        cJSON_AddItemToArray(sched, entry);
    }
    cJSON_AddItemToObject(root, "scheduler", sched);

    return root;
}

int json_to_state(app_t *app, const cJSON *json)
{
    cJSON *ro = cJSON_GetObjectItemCaseSensitive(json, "ro");
    if (!cJSON_IsObject(ro)) return -1;

    cJSON *instance_id_j = cJSON_GetObjectItemCaseSensitive(ro, "instance_id");
    cJSON *now_tick_j = cJSON_GetObjectItemCaseSensitive(ro, "now_tick");
    cJSON *now_unix_sec_j = cJSON_GetObjectItemCaseSensitive(ro, "now_unix_sec");
    if (!cJSON_IsString(instance_id_j) ||
        !cJSON_IsNumber(now_tick_j) || !cJSON_IsNumber(now_unix_sec_j)) return -1;

    app->now_tick = (uint64_t)now_tick_j->valuedouble;
    app->now_unix_sec = (uint64_t)now_unix_sec_j->valuedouble;

    cJSON *ch = cJSON_GetObjectItemCaseSensitive(ro, "character");
    if (ch == NULL) return -1; /* field must be present; null is valid, absence is not */
    if (cJSON_IsNull(ch)) {
        app->has_character = 0;
        scheduler_init(&app->scheduler);
        return 0;
    }
    if (!cJSON_IsObject(ch)) return -1;

    cJSON *id_j = cJSON_GetObjectItemCaseSensitive(ch, "id");
    cJSON *b_unix_j = cJSON_GetObjectItemCaseSensitive(ch, "birth_unix_sec");
    cJSON *b_tick_j = cJSON_GetObjectItemCaseSensitive(ch, "birth_tick");

    if (!cJSON_IsString(id_j) || !cJSON_IsNumber(b_unix_j) ||
        !cJSON_IsNumber(b_tick_j))
        return -1;

    app->character.id = (uint32_t)strtoul(id_j->valuestring, NULL, 16);
    app->character.birth_unix_sec = (uint64_t)b_unix_j->valuedouble;
    app->character.birth_tick = (uint64_t)b_tick_j->valuedouble;

    app->has_character = 1;

    scheduler_clear(&app->scheduler);
    return 0;
}
