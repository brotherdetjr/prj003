#include <stdio.h>
#include <string.h>
#include "lua_bind.h"
#include "../../vendor/lua/lua.h"
#include "../../vendor/lua/lualib.h"
#include "../../vendor/lua/lauxlib.h"
#include "../../vendor/cjson/cJSON.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Retrieve the app_t pointer stored in the Lua registry. */
static app_t *get_app(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "_gloxie_app");
    app_t *app = (app_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return app;
}

/* Find a free slot in app->lua_events[]. Returns -1 if full. */
static int alloc_event_slot(app_t *app)
{
    for (unsigned i = 0; i < LUA_MAX_EVENTS; i++) {
        if (app->lua_events[i].lua_ref == LUA_NOREF)
            return (int)i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* gloxie module functions                                              */
/* ------------------------------------------------------------------ */

/* gloxie.now_tick() → integer */
static int l_now_tick(lua_State *L)
{
    app_t *app = get_app(L);
    lua_pushinteger(L, (lua_Integer)app->world.now_tick);
    return 1;
}

/* gloxie.now_unix_sec() → integer */
static int l_now_unix_sec(lua_State *L)
{
    app_t *app = get_app(L);
    lua_pushinteger(L, (lua_Integer)app->world.now_unix_sec);
    return 1;
}

/* gloxie.character() → table {id, birth_unix_sec, birth_tick} or nil */
static int l_character(lua_State *L)
{
    app_t *app = get_app(L);
    if (!app->world.has_character) {
        lua_pushnil(L);
        return 1;
    }
    const character_t *ch = &app->world.character;
    lua_createtable(L, 0, 3);
    char id_str[9];
    snprintf(id_str, sizeof(id_str), "%08X", ch->id);
    lua_pushstring(L, id_str);
    lua_setfield(L, -2, "id");
    lua_pushinteger(L, (lua_Integer)ch->birth_unix_sec);
    lua_setfield(L, -2, "birth_unix_sec");
    lua_pushinteger(L, (lua_Integer)ch->birth_tick);
    lua_setfield(L, -2, "birth_tick");
    return 1;
}

/*
 * gloxie.schedule(delay_ms, fn, name)
 * Schedule `fn` to fire after `delay_ms` virtual milliseconds.
 * `name` is used for SSE event type and logging.
 */
static int l_schedule(lua_State *L)
{
    app_t *app = get_app(L);
    lua_Integer delay = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    const char *name = luaL_checkstring(L, 3);

    if (delay < 0)
        return luaL_error(L, "schedule: delay_ms must be >= 0");

    int slot = alloc_event_slot(app);
    if (slot < 0)
        return luaL_error(L, "schedule: lua_events table full");

    /* Store function in Lua registry */
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    app->lua_events[slot].lua_ref = ref;
    strncpy(app->lua_events[slot].name, name, sizeof(app->lua_events[slot].name) - 1);
    app->lua_events[slot].name[sizeof(app->lua_events[slot].name) - 1] = '\0';

    uint32_t tag = LUA_EVENT_BIT | (uint32_t)slot;
    scheduler_add(&app->world.scheduler,
                  app->world.now_tick + (uint64_t)delay, tag);
    return 0;
}

/* ------------------------------------------------------------------ */
/* scripted table → JSON and back                                       */
/* ------------------------------------------------------------------ */

/* Forward decl */
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

void lua_bind_flush_scripted(app_t *app)
{
    if (!app->world.has_character) return;
    lua_State *L = app->L;

    lua_getglobal(L, "gloxie");
    lua_getfield(L, -1, "scripted");

    cJSON *obj = lua_table_to_cjson(L, -1);
    char *s = cJSON_PrintUnformatted(obj);
    strncpy(app->world.character.scripted_json, s,
            CHARACTER_SCRIPTED_JSON_MAX - 1);
    app->world.character.scripted_json[CHARACTER_SCRIPTED_JSON_MAX - 1] = '\0';
    cJSON_free(s);
    cJSON_Delete(obj);

    lua_pop(L, 2); /* scripted, gloxie */
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

void lua_bind_reset_scripted(app_t *app)
{
    lua_State *L = app->L;
    lua_getglobal(L, "gloxie");
    lua_newtable(L);
    lua_setfield(L, -2, "scripted");
    lua_pop(L, 1);
}

void lua_bind_restore_scripted(app_t *app)
{
    if (!app->world.has_character) return;
    lua_State *L = app->L;
    const char *json = app->world.character.scripted_json;

    lua_getglobal(L, "gloxie");

    cJSON *obj = NULL;
    if (json && json[0] != '\0')
        obj = cJSON_Parse(json);

    if (obj && cJSON_IsObject(obj)) {
        cjson_to_lua_table(L, obj);
        cJSON_Delete(obj);
    } else {
        if (obj) cJSON_Delete(obj);
        lua_newtable(L);
    }
    lua_setfield(L, -2, "scripted");
    lua_pop(L, 1); /* gloxie */
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                             */
/* ------------------------------------------------------------------ */

void lua_bind_dispatch(uint32_t tag, void *ud)
{
    app_t *app = (app_t *)ud;
    if (!(tag & LUA_EVENT_BIT)) return;

    uint32_t slot = tag & ~LUA_EVENT_BIT;
    if (slot >= LUA_MAX_EVENTS) return;

    int ref = app->lua_events[slot].lua_ref;
    if (ref == LUA_NOREF) return;

    /* Capture name for caller before freeing the slot */
    strncpy(app->last_event_name, app->lua_events[slot].name,
            sizeof(app->last_event_name) - 1);
    app->last_event_name[sizeof(app->last_event_name) - 1] = '\0';

    /* Free the slot before calling the function — fn may reschedule itself */
    app->lua_events[slot].lua_ref = LUA_NOREF;
    app->lua_events[slot].name[0] = '\0';

    lua_State *L = app->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    luaL_unref(L, LUA_REGISTRYINDEX, ref);

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in event handler: %s\n",
                lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Call helper                                                          */
/* ------------------------------------------------------------------ */

void lua_bind_call0(app_t *app, const char *fn_name)
{
    lua_State *L = app->L;
    lua_getglobal(L, fn_name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "Lua error in %s: %s\n",
                fn_name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Init                                                                 */
/* ------------------------------------------------------------------ */

static const luaL_Reg gloxie_funcs[] = {
    {"now_tick",     l_now_tick},
    {"now_unix_sec", l_now_unix_sec},
    {"character",    l_character},
    {"schedule",     l_schedule},
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
    lua_setfield(L, LUA_REGISTRYINDEX, "_gloxie_app");

    /* Create gloxie module table */
    lua_newtable(L);
    luaL_setfuncs(L, gloxie_funcs, 0);

    /* gloxie.scripted starts as an empty table; populated on spawn/restore */
    lua_newtable(L);
    lua_setfield(L, -2, "scripted");

    lua_setglobal(L, "gloxie");

    /* Initialise event slots */
    for (unsigned i = 0; i < LUA_MAX_EVENTS; i++) {
        app->lua_events[i].lua_ref = LUA_NOREF;
        app->lua_events[i].name[0] = '\0';
    }

    /* Wire world dispatch */
    app->world.dispatch_cb = lua_bind_dispatch;
    app->world.dispatch_ud = app;

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
