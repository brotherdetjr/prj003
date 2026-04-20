#include <stdio.h>
#include <string.h>
#include "lua_bind.h"
#include "../vendor/lua/lua.h"
#include "../vendor/lua/lualib.h"
#include "../vendor/lua/lauxlib.h"
#include "../vendor/cjson/cJSON.h"

/* Registry keys */
#define REG_APP "_gloxie_app"
#define REG_RW  "_gloxie_rw"
#define REG_API "_gloxie_api"

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
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

/* Push the api table onto the Lua stack. */
static void push_api(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, REG_API);
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
/* api table functions (passed as 3rd arg to every callback)           */
/* ------------------------------------------------------------------ */

/*
 * api.schedule(delay_ms, event_name)
 * Schedule a call to the Lua global `event_name(ro, rw, api)` after
 * `delay_ms` virtual milliseconds. `event_name` is also used as
 * the SSE event type when the scheduler fires it.
 */
static int l_schedule(lua_State *L)
{
    app_t *app = get_app(L);
    lua_Integer delay = luaL_checkinteger(L, 1);
    const char *name  = luaL_checkstring(L, 2);

    if (delay < 0)
        return luaL_error(L, "schedule: delay_ms must be >= 0");

    int slot = alloc_event_slot(app);
    if (slot < 0)
        return luaL_error(L, "schedule: lua_events table full");

    strncpy(app->lua_events[slot].name, name,
            sizeof(app->lua_events[slot].name) - 1);
    app->lua_events[slot].name[sizeof(app->lua_events[slot].name) - 1] = '\0';

    scheduler_add(&app->scheduler,
                  app->now_tick + (uint64_t)delay, (uint32_t)slot);
    return 0;
}

/* ------------------------------------------------------------------ */
/* rw table → JSON and back                                            */
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
    cJSON_ArrayForEach(item, obj) {
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
/* Scheduler restore                                                    */
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
    cJSON_ArrayForEach(entry, arr) {
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
/* Dispatch                                                             */
/* ------------------------------------------------------------------ */

void lua_bind_dispatch(uint32_t tag, app_t *app)
{
    uint32_t slot = tag;
    if (slot >= LUA_MAX_EVENTS || app->lua_events[slot].name[0] == '\0') return;

    char name[32];
    strncpy(name, app->lua_events[slot].name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    /* Capture for caller; free slot before calling fn (fn may reschedule) */
    strncpy(app->last_event_name, name, sizeof(app->last_event_name) - 1);
    app->last_event_name[sizeof(app->last_event_name) - 1] = '\0';
    app->lua_events[slot].name[0] = '\0';

    lua_State *L = app->L;
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "Lua: no global function '%s' for scheduled event\n", name);
        lua_pop(L, 1);
        return;
    }
    push_api(L);
    push_rw(L);
    push_ro(L, app);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in %s: %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Call helper                                                          */
/* ------------------------------------------------------------------ */

void lua_bind_call(app_t *app, const char *fn_name)
{
    lua_State *L = app->L;
    lua_getglobal(L, fn_name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    push_api(L);
    push_rw(L);
    push_ro(L, app);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in %s: %s\n", fn_name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Init                                                                 */
/* ------------------------------------------------------------------ */

static const luaL_Reg api_funcs[] = {
    {"schedule", l_schedule},
    {NULL, NULL}
};

int lua_bind_init(app_t *app, const char *script_path)
{
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "lua_bind_init: luaL_newstate failed\n");
        return -1;
    }
    luaL_openlibs(L);

    /* Store app pointer in registry */
    lua_pushlightuserdata(L, app);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_APP);

    /* Build api table and store in registry; passed as 3rd arg to callbacks */
    lua_newtable(L);
    luaL_setfuncs(L, api_funcs, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, REG_API);

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
