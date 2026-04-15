#ifndef LUA_BIND_H
#define LUA_BIND_H

#include "../common/app.h"
#include "../../vendor/cjson/cJSON.h"

/*
 * Initialise the Lua VM for `app`, load the script at `script_path`,
 * and wire up world dispatch to the Lua event table.
 * Returns 0 on success, -1 on error (message printed to stderr).
 */
int lua_bind_init(app_t *app, const char *script_path);

/*
 * Call the global Lua function `fn_name(gloxie)`.
 * Non-existent functions are silently ignored; errors go to stderr.
 */
void lua_bind_call(app_t *app, const char *fn_name);

/*
 * Serialise gloxie.scripted to a JSON string and write it into
 * app->world.character.scripted_json.
 * Call before saving state.
 */
void lua_bind_flush_scripted(app_t *app);

/*
 * Reset gloxie.scripted to an empty table.
 * Call before on_spawn() so the script starts with a clean slate.
 */
void lua_bind_reset_scripted(app_t *app);

/*
 * Populate gloxie.scripted from app->world.character.scripted_json.
 * Call after restoring state (set_state / --file load).
 */
void lua_bind_restore_scripted(app_t *app);

/*
 * Restore the scheduler from a JSON array of {fire_at_ms, event} objects.
 * Clears both lua_events[] and the scheduler first, then repopulates.
 * Passing NULL or a non-array is a no-op (leaves scheduler empty).
 */
void lua_bind_restore_scheduler(app_t *app, const cJSON *arr);

/*
 * Dispatch callback wired into world_t.dispatch_cb.
 * Calls the named Lua global, passing the gloxie table as first argument.
 */
void lua_bind_dispatch(uint32_t tag, void *ud);

#endif /* LUA_BIND_H */
