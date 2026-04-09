#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"

cJSON *app_state_to_json(const app_t *app)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "instance_id",  app->instance_id);
    cJSON_AddNumberToObject(root, "now_tick",      (double)app->world.now_tick);
    cJSON_AddNumberToObject(root, "now_unix_ms",   (double)app->world.now_unix_ms);
    cJSON_AddBoolToObject  (root, "autotick",      app->autotick);

    if (app->world.has_character) {
        const character_t *ch = &app->world.character;
        char id_str[9];
        snprintf(id_str, sizeof(id_str), "%08X", ch->id);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "id",            id_str);
        cJSON_AddNumberToObject(c, "birth_unix_ms", (double)ch->birth_unix_ms);
        cJSON_AddNumberToObject(c, "birth_tick", (double)ch->birth_tick);
        cJSON_AddNumberToObject(c, "energy",        ch->energy);
        cJSON_AddItemToObject(root, "character", c);
    } else {
        cJSON_AddNullToObject(root, "character");
    }

    return root;
}

int json_to_world(world_t *w, const cJSON *json)
{
    cJSON *now_tick_j    = cJSON_GetObjectItemCaseSensitive(json, "now_tick");
    cJSON *now_unix_ms_j = cJSON_GetObjectItemCaseSensitive(json, "now_unix_ms");
    if (!cJSON_IsNumber(now_tick_j) || !cJSON_IsNumber(now_unix_ms_j)) return -1;

    w->now_tick    = (uint64_t)now_tick_j->valuedouble;
    w->now_unix_ms = (uint64_t)now_unix_ms_j->valuedouble;

    cJSON *ch = cJSON_GetObjectItemCaseSensitive(json, "character");
    if (cJSON_IsNull(ch) || ch == NULL) {
        w->has_character = 0;
        scheduler_init(&w->scheduler);
        return 0;
    }
    if (!cJSON_IsObject(ch)) return -1;

    cJSON *id_j       = cJSON_GetObjectItemCaseSensitive(ch, "id");
    cJSON *b_unix_j   = cJSON_GetObjectItemCaseSensitive(ch, "birth_unix_ms");
    cJSON *b_tick_j   = cJSON_GetObjectItemCaseSensitive(ch, "birth_tick");
    cJSON *energy_j   = cJSON_GetObjectItemCaseSensitive(ch, "energy");

    if (!cJSON_IsString(id_j)     || !cJSON_IsNumber(b_unix_j) ||
        !cJSON_IsNumber(b_tick_j) || !cJSON_IsNumber(energy_j))
        return -1;

    w->character.id            = (uint32_t)strtoul(id_j->valuestring, NULL, 16);
    w->character.birth_unix_ms = (uint64_t)b_unix_j->valuedouble;
    w->character.birth_tick = (uint64_t)b_tick_j->valuedouble;
    w->character.energy        = (uint8_t) energy_j->valueint;
    w->has_character = 1;

    world_rebuild_scheduler(w);
    return 0;
}
