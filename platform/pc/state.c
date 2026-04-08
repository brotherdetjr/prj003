#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"

cJSON *app_state_to_json(const app_t *app)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "instance_id", app->instance_id);
    cJSON_AddNumberToObject(root, "now_ts",  (double)app->world.now_ts);
    cJSON_AddBoolToObject  (root, "autotick", app->autotick);

    if (app->world.has_character) {
        const character_t *c = &app->world.character;
        char id_str[9];
        snprintf(id_str, sizeof(id_str), "%08X", c->id);
        cJSON *ch = cJSON_CreateObject();
        cJSON_AddStringToObject(ch, "id",        id_str);
        cJSON_AddNumberToObject(ch, "birth_ts",  (double)c->birth_ts);
        cJSON_AddNumberToObject(ch, "energy",    c->energy);
        cJSON_AddNumberToObject(ch, "drain_acc", c->_drain_acc);
        cJSON_AddItemToObject(root, "character", ch);
    } else {
        cJSON_AddNullToObject(root, "character");
    }

    return root;
}

int json_to_world(world_t *w, const cJSON *json)
{
    cJSON *now_ts_j = cJSON_GetObjectItemCaseSensitive(json, "now_ts");
    if (!cJSON_IsNumber(now_ts_j)) return -1;
    w->now_ts = (uint64_t)now_ts_j->valuedouble;

    cJSON *ch = cJSON_GetObjectItemCaseSensitive(json, "character");
    if (cJSON_IsNull(ch) || ch == NULL) {
        w->has_character = 0;
        return 0;
    }
    if (!cJSON_IsObject(ch)) return -1;

    cJSON *id_j     = cJSON_GetObjectItemCaseSensitive(ch, "id");
    cJSON *birth_j  = cJSON_GetObjectItemCaseSensitive(ch, "birth_ts");
    cJSON *energy_j = cJSON_GetObjectItemCaseSensitive(ch, "energy");
    cJSON *drain_j  = cJSON_GetObjectItemCaseSensitive(ch, "drain_acc");

    if (!cJSON_IsString(id_j) || !cJSON_IsNumber(birth_j) ||
        !cJSON_IsNumber(energy_j) || !cJSON_IsNumber(drain_j))
        return -1;

    w->character.id         = (uint32_t)strtoul(id_j->valuestring, NULL, 16);
    w->character.birth_ts   = (uint64_t)birth_j->valuedouble;
    w->character.energy     = (uint8_t) energy_j->valueint;
    w->character._drain_acc = (uint16_t)drain_j->valueint;
    w->has_character = 1;
    return 0;
}
