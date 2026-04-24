#ifndef LUA_BIND_H
#define LUA_BIND_H

#include "app.h"
#include "../vendor/cjson/cJSON.h"

/*
 * Initialise the Lua VM for `app`, load the script at `script_path`,
 * and wire up world dispatch to the Lua event table.
 * Returns 0 on success, -1 on error (message printed to stderr).
 */
int lua_bind_init(app_t *app, const char *script_path);

/*
 * Call the global Lua function `fn_name(ro, rw)`.
 * Non-existent functions are silently ignored; errors go to stderr.
 */
void lua_bind_call(app_t *app, const char *fn_name);

/*
 * Return the rw table as a cJSON object (caller must cJSON_Delete).
 * Used by app_state_to_json to embed rw state without an intermediate
 * string buffer.
 */
cJSON *lua_bind_rw_to_cjson(app_t *app);

/*
 * Reset the rw table to an empty table.
 * Call before on_spawn() so the script starts with a clean slate.
 */
void lua_bind_reset_rw(app_t *app);

/*
 * Restore full Lua state (rw + scheduler) from a state JSON object.
 * Call after json_to_state() when loading state from --file or set_state.
 * Returns 0 on success, -1 if the scheduler array contains invalid entries.
 */
int lua_bind_restore(app_t *app, const cJSON *state_json);

/*
 * Reload the Lua script from script_path, preserving rw and scheduler state.
 * Returns 0 on success. On load error the old Lua state is kept intact and
 * -1 is returned (error already printed to stderr).
 */
int lua_bind_reload(app_t *app, const char *script_path);

/*
 * Dispatch callback wired into app_t.dispatch_cb.
 * Calls the named Lua global, passing ro and rw as arguments.
 */
void lua_bind_dispatch(uint32_t tag, app_t *app);

#endif /* LUA_BIND_H */
