#ifndef STATE_H
#define STATE_H

#include "app.h"
#include "../../vendor/cjson/cJSON.h"

/* Serialise full app state to a cJSON object (caller must cJSON_Delete). */
cJSON *app_state_to_json(const app_t *app);

/*
 * Deserialise world state (now_ts + character) from a cJSON State object.
 * instance_id and autotick are not restored — they are runtime settings.
 * Returns 0 on success, -1 on malformed input.
 */
int json_to_world(world_t *w, const cJSON *json);

#endif /* STATE_H */
