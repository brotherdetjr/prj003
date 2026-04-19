#ifndef STATE_H
#define STATE_H

#include "app.h"
#include "../vendor/cjson/cJSON.h"

/* Serialise full app state to a cJSON object (caller must cJSON_Delete). */
cJSON *app_state_to_json(app_t *app);

/*
 * Deserialise world state (now_tick + character) from a cJSON State object.
 * instance_id, autotick, and dispatch_cb are not restored — they are runtime settings.
 * Returns 0 on success, -1 on malformed input.
 */
int json_to_world(app_t *app, const cJSON *json);

#endif /* STATE_H */
