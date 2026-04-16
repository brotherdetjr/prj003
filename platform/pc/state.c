#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "lua_bind.h"

cJSON *app_state_to_json(app_t *app)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "instance_id",  app->instance_id);
    cJSON_AddStringToObject(root, "script",       app->script_path);
    cJSON_AddNumberToObject(root, "now_tick",      (double)app->world.now_tick);
    cJSON_AddNumberToObject(root, "now_unix_sec",   (double)app->world.now_unix_sec);
    cJSON_AddBoolToObject  (root, "autotick",      app->autotick);

    if (app->world.has_character) {
        const character_t *ch = &app->world.character;
        char id_str[9];
        snprintf(id_str, sizeof(id_str), "%08X", ch->id);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "id",            id_str);
        cJSON_AddNumberToObject(c, "birth_unix_sec", (double)ch->birth_unix_sec);
        cJSON_AddNumberToObject(c, "birth_tick",     (double)ch->birth_tick);

        cJSON *scripted = app->L ? lua_bind_scripted_to_cjson(app) : cJSON_CreateObject();
        cJSON_AddItemToObject(c, "scripted", scripted);

        cJSON_AddItemToObject(root, "character", c);
    } else {
        cJSON_AddNullToObject(root, "character");
    }

    /* Scheduler: array of {fire_at_ms, event} for each pending Lua event */
    cJSON *sched = cJSON_CreateArray();
    for (int i = 0; i < app->world.scheduler.count; i++) {
        const scheduled_event_t *ev = &app->world.scheduler.heap[i];
        if (!(ev->tag & LUA_EVENT_BIT)) continue;
        uint32_t slot = ev->tag & ~LUA_EVENT_BIT;
        if (slot >= LUA_MAX_EVENTS || app->lua_events[slot].name[0] == '\0') continue;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "fire_at_ms", (double)ev->fire_at_ms);
        cJSON_AddStringToObject(entry, "event",      app->lua_events[slot].name);
        cJSON_AddItemToArray(sched, entry);
    }
    cJSON_AddItemToObject(root, "scheduler", sched);

    return root;
}

int json_to_world(world_t *w, const cJSON *json)
{
    cJSON *now_tick_j    = cJSON_GetObjectItemCaseSensitive(json, "now_tick");
    cJSON *now_unix_sec_j = cJSON_GetObjectItemCaseSensitive(json, "now_unix_sec");
    if (!cJSON_IsNumber(now_tick_j) || !cJSON_IsNumber(now_unix_sec_j)) return -1;

    w->now_tick    = (uint64_t)now_tick_j->valuedouble;
    w->now_unix_sec = (uint64_t)now_unix_sec_j->valuedouble;

    cJSON *ch = cJSON_GetObjectItemCaseSensitive(json, "character");
    if (cJSON_IsNull(ch) || ch == NULL) {
        w->has_character = 0;
        scheduler_init(&w->scheduler);
        return 0;
    }
    if (!cJSON_IsObject(ch)) return -1;

    cJSON *id_j     = cJSON_GetObjectItemCaseSensitive(ch, "id");
    cJSON *b_unix_j = cJSON_GetObjectItemCaseSensitive(ch, "birth_unix_sec");
    cJSON *b_tick_j = cJSON_GetObjectItemCaseSensitive(ch, "birth_tick");

    if (!cJSON_IsString(id_j)     || !cJSON_IsNumber(b_unix_j) ||
        !cJSON_IsNumber(b_tick_j))
        return -1;

    w->character.id             = (uint32_t)strtoul(id_j->valuestring, NULL, 16);
    w->character.birth_unix_sec  = (uint64_t)b_unix_j->valuedouble;
    w->character.birth_tick      = (uint64_t)b_tick_j->valuedouble;

    w->has_character = 1;

    scheduler_clear(&w->scheduler);
    return 0;
}
