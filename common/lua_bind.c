#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "lua_bind.h"
#include "state.h"
#include "../vendor/lua/lua.h"
#include "../vendor/lua/lualib.h"
#include "../vendor/lua/lauxlib.h"
#include "../vendor/cjson/cJSON.h"

/* Registry keys */
#define REG_APP "_gloxie_app"
#define REG_RW "_gloxie_rw"
#define REG_PREFIX "_gloxie_prefix"

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static app_t *get_app(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, REG_APP);
    app_t *app = (app_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return app;
}

/* Push the rw table onto the Lua stack. */
static void push_rw(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, REG_RW);
}

/* Push a fresh ro snapshot table onto the Lua stack. */
static void push_ro(lua_State *L, app_t *app)
{
    lua_createtable(L, 0, 4);
    lua_pushstring(L, app->instance_id);
    lua_setfield(L, -2, "instance_id");
    lua_pushinteger(L, (lua_Integer)app->now_tick);
    lua_setfield(L, -2, "now_tick");
    lua_pushinteger(L, (lua_Integer)app->now_unix_sec);
    lua_setfield(L, -2, "now_unix_sec");

    if (app->has_character) {
        const character_t *ch = &app->character;
        lua_createtable(L, 0, 3);
        char id_str[9];
        snprintf(id_str, sizeof(id_str), "%08X", ch->id);
        lua_pushstring(L, id_str);
        lua_setfield(L, -2, "id");
        lua_pushinteger(L, (lua_Integer)ch->birth_unix_sec);
        lua_setfield(L, -2, "birth_unix_sec");
        lua_pushinteger(L, (lua_Integer)ch->birth_tick);
        lua_setfield(L, -2, "birth_tick");
        lua_setfield(L, -2, "character");
    } else {
        lua_pushnil(L);
        lua_setfield(L, -2, "character");
    }
}

/* Find a free slot in app->lua_events[]. Returns -1 if full. */
static int alloc_event_slot(app_t *app)
{
    for (unsigned i = 0; i < LUA_MAX_EVENTS; i++) {
        if (app->lua_events[i].name[0] == '\0')
            return (int)i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Global functions exposed to Lua                                    */
/* ------------------------------------------------------------------ */

static void set_schedule_prefix(lua_State *L, const char *prefix)
{
    lua_pushstring(L, prefix);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_PREFIX);
}

/*
 * Scan _G at depth 1 and 2 for the table at stack index mod_idx.
 * On success writes "path." into buf (e.g. "nrg." or "myapp.energy.") and
 * returns 1.  Returns 0 if not found.  Stack is balanced on return.
 * "package" is excluded from depth-2 to avoid false matches via
 * package.loaded.
 */
static int find_global_path(lua_State *L, int mod_idx, char *buf,
                            size_t buf_size)
{
    if (mod_idx < 0) mod_idx = lua_gettop(L) + 1 + mod_idx;
    buf[0] = '\0';

    lua_pushglobaltable(L);
    int g = lua_gettop(L);

    lua_pushnil(L);
    while (lua_next(L, g)) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_rawequal(L, mod_idx, -1)) {
            snprintf(buf, buf_size, "%s.", lua_tostring(L, -2));
            lua_pop(L, 2);
            goto done;
        }
        lua_pop(L, 1);
    }

    lua_pushnil(L);
    while (lua_next(L, g)) {
        if (lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TTABLE ||
            lua_rawequal(L, -1, g) || strcmp(lua_tostring(L, -2), "package") == 0) {
            lua_pop(L, 1);
            continue;
        }
        char k1[64];
        strncpy(k1, lua_tostring(L, -2), sizeof(k1) - 1);
        k1[sizeof(k1) - 1] = '\0';
        int t = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, t)) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_rawequal(L, mod_idx, -1)) {
                snprintf(buf, buf_size, "%s.%s.", k1, lua_tostring(L, -2));
                lua_pop(L, 2); /* inner k, v */
                lua_pop(L, 2); /* outer table, k1 */
                goto done;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

done:
    lua_pop(L, 1); /* _G */
    return buf[0] != '\0';
}

/*
 * Walk the Lua call stack to find the innermost Lua source file, map it to
 * a module table via package.loaded, then scan _G to find where that table
 * is actually mounted (so the prefix matches the real global path, not the
 * require() argument).  Pushes the prefix string — e.g. "nrg.",
 * "myapp.energy.", or "" for top-level callers — onto the stack.
 */
static void push_caller_prefix(lua_State *L)
{
    lua_Debug ar;
    char src[1024] = "";
    for (int level = 1; lua_getstack(L, level, &ar); level++) {
        lua_getinfo(L, "S", &ar);
        if (ar.what[0] == 'L' && ar.source[0] == '@') {
            strncpy(src, ar.source + 1, sizeof(src) - 1);
            break;
        }
    }
    if (!src[0]) {
        lua_pushstring(L, "");
        return;
    }

    lua_getglobal(L, "package");

    lua_getfield(L, -1, "path");
    const char *pp = lua_tostring(L, -1);
    char dir[1024] = "";
    if (pp) {
        const char *q = strstr(pp, "/?.lua");
        if (q && (size_t)(q - pp) < sizeof(dir)) {
            size_t n = (size_t)(q - pp);
            memcpy(dir, pp, n);
            dir[n] = '\0';
        }
    }
    lua_pop(L, 1); /* path */
    if (!dir[0]) {
        lua_pop(L, 1);
        lua_pushstring(L, "");
        return;
    }

    lua_getfield(L, -1, "loaded");
    lua_remove(L, -2); /* drop package */
    int loaded = lua_gettop(L);

    lua_pushnil(L);
    while (lua_next(L, loaded)) {
        if (lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TTABLE) {
            lua_pop(L, 1);
            continue;
        }
        const char *mn = lua_tostring(L, -2);
        char rp[256];
        strncpy(rp, mn, sizeof(rp) - 1);
        rp[sizeof(rp) - 1] = '\0';
        for (char *p = rp; *p; p++)
            if (*p == '.') *p = '/';
        char fp[1024];
        int n = snprintf(fp, sizeof(fp), "%s/%s.lua", dir, rp);
        if (n > 0 && (size_t)n < sizeof(fp) && strcmp(fp, src) == 0) {
            lua_remove(L, -2); /* drop key, leave module table on top */
            goto found;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1); /* loaded */
    lua_pushstring(L, "");
    return;

found:;
    char prefix[128] = "";
    find_global_path(L, -1, prefix, sizeof(prefix));
    lua_pop(L, 2); /* module_table, loaded */
    lua_pushstring(L, prefix);
}

/*
 * schedule(delay_ms, event_name)
 * Schedule a call to the Lua function `event_name` after `delay_ms` virtual
 * milliseconds. `event_name` is relative to the current module: the dispatch
 * layer prepends the module prefix so modules never need to know their own
 * path in the global table hierarchy. `event_name` is also used as the SSE
 * event type when the scheduler fires it.
 */
static int l_schedule(lua_State *L)
{
    app_t *app = get_app(L);
    lua_Integer delay = luaL_checkinteger(L, 1);
    const char *name = luaL_checkstring(L, 2);

    if (delay < 0)
        return luaL_error(L, "schedule: delay_ms must be >= 0");

    if (name[0] == '_')
        return luaL_error(L, "schedule: event name must not start with '_'");

    /*
     * Determine the module prefix. Prefer the dispatch-set prefix (non-empty
     * when inside a scheduled callback — required for the diamond-require case
     * where the same module lives under multiple paths). Fall back to
     * caller-file detection so helper functions like module.init() schedule
     * correctly relative to their own module without knowing its path.
     */
    char prefix_buf[sizeof(app->lua_events[0].name)] = "";
    lua_getfield(L, LUA_REGISTRYINDEX, REG_PREFIX);
    if (lua_isstring(L, -1)) {
        const char *dp = lua_tostring(L, -1);
        strncpy(prefix_buf, dp, sizeof(prefix_buf) - 1);
    }
    lua_pop(L, 1);
    if (prefix_buf[0] == '\0') {
        push_caller_prefix(L);
        const char *cp = lua_tostring(L, -1);
        if (cp)
            strncpy(prefix_buf, cp, sizeof(prefix_buf) - 1);
        lua_pop(L, 1);
    }

    char full_name[sizeof(app->lua_events[0].name)];
    int n = snprintf(full_name, sizeof(full_name), "%s%s", prefix_buf, name);

    if (n < 0 || (size_t)n >= sizeof(full_name))
        return luaL_error(L, "schedule: event name too long (max %d chars)",
                          (int)(sizeof(full_name) - 1));

    int slot = alloc_event_slot(app);
    if (slot < 0)
        return luaL_error(L, "schedule: lua_events table full");

    strncpy(app->lua_events[slot].name, full_name,
            sizeof(app->lua_events[slot].name) - 1);
    app->lua_events[slot].name[sizeof(app->lua_events[slot].name) - 1] = '\0';

    scheduler_add(&app->scheduler,
                  app->now_tick + (uint64_t)delay, (uint32_t)slot);
    return 0;
}

/* ------------------------------------------------------------------ */
/* rw table → JSON and back                                           */
/* ------------------------------------------------------------------ */

static cJSON *lua_table_to_cjson(lua_State *L, int idx);

static cJSON *lua_value_to_cjson(lua_State *L, int idx)
{
    switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN:
        return lua_toboolean(L, idx) ? cJSON_CreateTrue() : cJSON_CreateFalse();
    case LUA_TNUMBER:
        if (lua_isinteger(L, idx))
            return cJSON_CreateNumber((double)lua_tointeger(L, idx));
        return cJSON_CreateNumber(lua_tonumber(L, idx));
    case LUA_TSTRING:
        return cJSON_CreateString(lua_tostring(L, idx));
    case LUA_TTABLE:
        return lua_table_to_cjson(L, idx);
    default:
        return cJSON_CreateNull();
    }
}

static cJSON *lua_table_to_cjson(lua_State *L, int idx)
{
    if (idx < 0) idx = lua_gettop(L) + 1 + idx;
    cJSON *obj = cJSON_CreateObject();
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            cJSON *val = lua_value_to_cjson(L, -1);
            cJSON_AddItemToObject(obj, lua_tostring(L, -2), val);
        }
        lua_pop(L, 1);
    }
    return obj;
}

cJSON *lua_bind_rw_to_cjson(app_t *app)
{
    lua_State *L = app->L;
    push_rw(L);
    cJSON *obj = lua_table_to_cjson(L, -1);
    lua_pop(L, 1);
    return obj;
}

void lua_bind_reset_rw(app_t *app)
{
    lua_State *L = app->L;
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_RW);
}

static void cjson_to_lua_table(lua_State *L, const cJSON *obj)
{
    lua_newtable(L);
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, obj)
    {
        if (!item->string) continue;
        if (cJSON_IsNumber(item)) {
            double v = item->valuedouble;
            long long iv = (long long)v;
            if ((double)iv == v)
                lua_pushinteger(L, (lua_Integer)iv);
            else
                lua_pushnumber(L, v);
        } else if (cJSON_IsString(item)) {
            lua_pushstring(L, item->valuestring);
        } else if (cJSON_IsBool(item)) {
            lua_pushboolean(L, cJSON_IsTrue(item));
        } else if (cJSON_IsObject(item)) {
            cjson_to_lua_table(L, item);
        } else {
            lua_pushnil(L);
        }
        lua_setfield(L, -2, item->string);
    }
}

static void lua_bind_restore_rw(app_t *app, const cJSON *rw_json)
{
    lua_State *L = app->L;
    if (rw_json && cJSON_IsObject(rw_json))
        cjson_to_lua_table(L, rw_json);
    else
        lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_RW);
}

/* ------------------------------------------------------------------ */
/* Scheduler restore                                                  */
/* ------------------------------------------------------------------ */

static int lua_bind_restore_scheduler(app_t *app, const cJSON *arr)
{
    /* Clear all slots and the scheduler */
    for (unsigned i = 0; i < LUA_MAX_EVENTS; i++)
        app->lua_events[i].name[0] = '\0';
    scheduler_clear(&app->scheduler);

    if (cJSON_IsNull(arr) || arr == NULL) return 0;
    if (!cJSON_IsArray(arr)) return -1;

    const cJSON *entry;
    cJSON_ArrayForEach(entry, arr)
    {
        cJSON *fire_j = cJSON_GetObjectItemCaseSensitive(entry, "fire_at_ms");
        cJSON *name_j = cJSON_GetObjectItemCaseSensitive(entry, "event");
        if (!cJSON_IsNumber(fire_j) || !cJSON_IsString(name_j)) return -1;

        int slot = alloc_event_slot(app);
        if (slot < 0) {
            fprintf(stderr, "lua_bind_restore_scheduler: event table full\n");
            return -1;
        }

        strncpy(app->lua_events[slot].name, name_j->valuestring,
                sizeof(app->lua_events[slot].name) - 1);
        app->lua_events[slot].name[sizeof(app->lua_events[slot].name) - 1] = '\0';

        uint64_t fire_at_ms = (uint64_t)fire_j->valuedouble;
        scheduler_add(&app->scheduler, fire_at_ms, (uint32_t)slot);
    }
    return 0;
}

int lua_bind_restore(app_t *app, const cJSON *state_json)
{
    lua_bind_restore_rw(app,
                        cJSON_GetObjectItemCaseSensitive(state_json, "rw"));
    return lua_bind_restore_scheduler(app,
                                      cJSON_GetObjectItemCaseSensitive(state_json, "scheduler"));
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                           */
/* ------------------------------------------------------------------ */

/*
 * Push the value reached by a dot-separated path of global table keys.
 * "foo"         -> _G["foo"]
 * "foo.bar.baz" -> _G["foo"]["bar"]["baz"]
 * Leaves exactly one value on the stack (nil if any step fails).
 */
static void lua_push_by_path(lua_State *L, const char *path)
{
    char buf[64];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *p = buf;
    char *dot = strchr(p, '.');
    if (dot) *dot = '\0';
    lua_getglobal(L, p);
    if (!dot) return;

    p = dot + 1;
    while (1) {
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_pushnil(L);
            return;
        }
        dot = strchr(p, '.');
        if (dot) *dot = '\0';
        lua_getfield(L, -1, p);
        lua_remove(L, -2);
        if (!dot) return;
        p = dot + 1;
    }
}

void lua_bind_dispatch(uint32_t tag, app_t *app)
{
    uint32_t slot = tag;
    if (slot >= LUA_MAX_EVENTS || app->lua_events[slot].name[0] == '\0') return;

    char name[64];
    strncpy(name, app->lua_events[slot].name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    /* Capture for caller; free slot before calling fn (fn may reschedule) */
    strncpy(app->last_event_name, name, sizeof(app->last_event_name) - 1);
    app->last_event_name[sizeof(app->last_event_name) - 1] = '\0';
    app->lua_events[slot].name[0] = '\0';

    /* Extract module prefix: "effects.drain.on_drain" -> "effects.drain." */
    char prefix[64] = "";
    const char *last_dot = strrchr(name, '.');
    if (last_dot) {
        size_t plen = (size_t)(last_dot - name + 1);
        memcpy(prefix, name, plen);
        prefix[plen] = '\0';
    }

    lua_State *L = app->L;
    set_schedule_prefix(L, prefix);

    lua_push_by_path(L, name);
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "Lua: no function '%s' for scheduled event\n", name);
        lua_pop(L, 1);
        set_schedule_prefix(L, "");
        return;
    }
    push_rw(L);
    push_ro(L, app);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in %s: %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    set_schedule_prefix(L, "");
}

/* ------------------------------------------------------------------ */
/* Call helper                                                        */
/* ------------------------------------------------------------------ */

void lua_bind_call(app_t *app, const char *fn_name)
{
    lua_State *L = app->L;
    set_schedule_prefix(L, "");
    lua_push_by_path(L, fn_name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    push_rw(L);
    push_ro(L, app);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in %s: %s\n", fn_name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Reload                                                             */
/* ------------------------------------------------------------------ */

int lua_bind_reload(app_t *app, const char *script_path)
{
    cJSON *snap = app_state_to_json(app); /* lua_bind_restore ignores "ro" */

    /* Stash old state; lua_bind_init clears lua_events so save them too */
    lua_State *old_L = app->L;
    lua_event_t saved_events[LUA_MAX_EVENTS];
    memcpy(saved_events, app->lua_events, sizeof(saved_events));

    app->L = NULL;
    if (lua_bind_init(app, script_path) != 0) {
        /* Load failed — restore old state intact */
        app->L = old_L;
        memcpy(app->lua_events, saved_events, sizeof(saved_events));
        cJSON_Delete(snap);
        return -1;
    }

    lua_close(old_L);
    lua_bind_restore(app, snap);
    cJSON_Delete(snap);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Loaded-file enumeration                                            */
/* ------------------------------------------------------------------ */

int lua_bind_get_loaded_files(app_t *app, char (*out)[1024], int max_count)
{
    lua_State *L = app->L;
    if (!L || max_count <= 0) return 0;

    /* Derive script directory from package.path = "<dir>/?.lua" */
    char dir[1024] = "";
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char *pkg_path = lua_tostring(L, -1);
    if (pkg_path) {
        const char *q = strstr(pkg_path, "/?.lua");
        if (q) {
            size_t n = (size_t)(q - pkg_path);
            if (n < sizeof(dir)) {
                memcpy(dir, pkg_path, n);
                dir[n] = '\0';
            }
        }
    }
    lua_pop(L, 2); /* path, package */

    if (dir[0] == '\0') return 0;

    int count = 0;

    /* Walk package.loaded; include entries that resolve to real .lua files */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushnil(L);
    while (count < max_count && lua_next(L, -2)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char *modname = lua_tostring(L, -2);
            /* Lua module convention: dots become directory separators */
            char relpath[256];
            strncpy(relpath, modname, sizeof(relpath) - 1);
            relpath[sizeof(relpath) - 1] = '\0';
            for (char *p = relpath; *p; p++)
                if (*p == '.') *p = '/';

            char fullpath[1024];
            int n = snprintf(fullpath, sizeof(fullpath),
                             "%s/%s.lua", dir, relpath);
            if (n > 0 && (size_t)n < sizeof(fullpath)) {
                struct stat st;
                if (stat(fullpath, &st) == 0) {
                    strncpy(out[count], fullpath, 1023);
                    out[count][1023] = '\0';
                    count++;
                }
            }
        }
        lua_pop(L, 1); /* pop value; keep key for lua_next */
    }
    lua_pop(L, 2); /* loaded, package */

    return count;
}

/* ------------------------------------------------------------------ */
/* Init                                                               */
/* ------------------------------------------------------------------ */

static void setup_package(lua_State *L, const char *script_path)
{
    /* Derive directory containing the main script */
    char dir[1024];
    const char *slash = strrchr(script_path, '/');
    if (slash) {
        size_t n = (size_t)(slash - script_path);
        if (n >= sizeof(dir)) n = sizeof(dir) - 1;
        memcpy(dir, script_path, n);
        dir[n] = '\0';
    } else {
        dir[0] = '.';
        dir[1] = '\0';
    }

    /* package.path = "<dir>/?.lua" */
    char pkg_path[1024 + 8];
    snprintf(pkg_path, sizeof(pkg_path), "%s/?.lua", dir);
    lua_getglobal(L, "package");
    lua_pushstring(L, pkg_path);
    lua_setfield(L, -2, "path");

    /* package.searchers = { package.searchers[2] }  — file loader only */
    lua_getfield(L, -1, "searchers"); /* package, searchers */
    lua_rawgeti(L, -1, 2);            /* package, searchers, file_searcher */
    lua_newtable(L);                  /* package, searchers, file_searcher, {} */
    lua_pushvalue(L, -2);             /* ..., {}, file_searcher */
    lua_rawseti(L, -2, 1);            /* ..., {file_searcher} */
    lua_setfield(L, -4, "searchers"); /* package.searchers = {file_searcher} */
    lua_pop(L, 3);                    /* clean */
}

int lua_bind_init(app_t *app, const char *script_path)
{
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "lua_bind_init: luaL_newstate failed\n");
        return -1;
    }
    luaL_openlibs(L);
    setup_package(L, script_path);

    /* Store app pointer in registry */
    lua_pushlightuserdata(L, app);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_APP);

    /* Register global functions */
    lua_register(L, "schedule", l_schedule);
    lua_pushstring(L, "");
    lua_setfield(L, LUA_REGISTRYINDEX, REG_PREFIX);

    /* Create rw table in the registry */
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_RW);

    /* Initialise event slots */
    for (unsigned i = 0; i < LUA_MAX_EVENTS; i++)
        app->lua_events[i].name[0] = '\0';

    /* Wire world dispatch */
    app->dispatch_cb = lua_bind_dispatch;

    app->L = L;

    /* Load game script */
    if (luaL_loadfile(L, script_path) != LUA_OK ||
        lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "lua_bind_init: failed to load %s: %s\n",
                script_path, lua_tostring(L, -1));
        lua_close(L);
        app->L = NULL;
        return -1;
    }

    return 0;
}
