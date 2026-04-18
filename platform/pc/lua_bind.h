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
 * Return gloxie.scripted as a cJSON object (caller must cJSON_Delete).
 * Used by app_state_to_json to embed scripted state without an intermediate
 * string buffer.
 */
cJSON *lua_bind_scripted_to_cjson(app_t *app);

/*
 * Reset gloxie.scripted to an empty table.
 * Call before _on_spawn() so the script starts with a clean slate.
 */
void lua_bind_reset_scripted(app_t *app);

/*
 * Restore full Lua state (scripted + scheduler) from a state JSON object.
 * Call after json_to_world() when loading state from --file or set_state.
 */
void lua_bind_restore(app_t *app, const cJSON *state_json);

/*
 * Dispatch callback wired into app_t.dispatch_cb.
 * Calls the named Lua global, passing the gloxie table as first argument.
 */
void lua_bind_dispatch(uint32_t tag, app_t *app);

#endif /* LUA_BIND_H */
