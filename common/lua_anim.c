#include "lua_anim.h"
#include "lua_gfx.h"
#include "gfx.h"
#include "spr.h"
#include "../vendor/lua/lauxlib.h"
#include "../vendor/cjson/cJSON.h"
#include <string.h>
#include <stdio.h>

#define ANIM_MAX 64
#define ANIM_ID_MAX 64
#define ANIM_PATH_MAX 1024
#define ANIM_KEY_PREFIX "_gloxie_anim_"
#define ANIM_KEY_MAX (sizeof(ANIM_KEY_PREFIX) - 1 + ANIM_ID_MAX)

typedef struct {
    char id[ANIM_ID_MAX];     /* empty = free slot */
    char path[ANIM_PATH_MAX]; /* resolved absolute path set by of() */
    int n_frames;             /* 0 = unset */
    int current_frame;        /* 1-based */
    int backwards;
    int playing;
    int loop;
    int used_in_last_draw;
} anim_entry_t;

static anim_entry_t s_anims[ANIM_MAX]; /* zero-initialised by C */

/* ------------------------------------------------------------------ */
/* C registry helpers                                                 */
/* ------------------------------------------------------------------ */

static anim_entry_t *anim_find(const char *id)
{
    for (int i = 0; i < ANIM_MAX; i++) {
        if (s_anims[i].id[0] != '\0' && strcmp(s_anims[i].id, id) == 0)
            return &s_anims[i];
    }
    return NULL;
}

static anim_entry_t *anim_alloc(const char *id)
{
    for (int i = 0; i < ANIM_MAX; i++) {
        if (s_anims[i].id[0] == '\0') {
            strncpy(s_anims[i].id, id, ANIM_ID_MAX - 1);
            s_anims[i].id[ANIM_ID_MAX - 1] = '\0';
            s_anims[i].path[0] = '\0';
            s_anims[i].n_frames = 0;
            s_anims[i].current_frame = 1;
            s_anims[i].backwards = 0;
            s_anims[i].playing = 1;
            s_anims[i].loop = 0;
            s_anims[i].used_in_last_draw = 0;
            return &s_anims[i];
        }
    }
    return NULL;
}

static void make_key(const char *id, char *out, size_t out_sz)
{
    snprintf(out, out_sz, ANIM_KEY_PREFIX "%s", id);
}

/* Push the method table for id from the Lua registry; return 1. */
static int push_self(lua_State *L, const char *id)
{
    char key[ANIM_KEY_MAX];
    make_key(id, key, sizeof(key));
    lua_getfield(L, LUA_REGISTRYINDEX, key);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Method implementations (each is a C closure with id as upvalue 1)  */
/* ------------------------------------------------------------------ */

static int l_anim_of(lua_State *L)
{
    const char *id = lua_tostring(L, lua_upvalueindex(1));
    const char *rel = luaL_checkstring(L, 1);

    anim_entry_t *e = anim_find(id);
    if (!e)
        return luaL_error(L, "anim.of: '%s' not found", id);
    if (e->path[0] != '\0')
        return luaL_error(L, "anim.of: '%s' already initialised", id);

    char abs_path[1024];
    lua_gfx_resolve_path(L, rel, abs_path, sizeof(abs_path));

    const char *err = NULL;
    int n = spr_frame_count(abs_path, &err);
    if (n < 0)
        return luaL_error(L, "anim.of: %s: %s", abs_path,
                          err ? err : "unknown error");

    strncpy(e->path, abs_path, ANIM_PATH_MAX - 1);
    e->path[ANIM_PATH_MAX - 1] = '\0';
    e->n_frames = n;
    return push_self(L, id);
}

static int l_anim_backwards(lua_State *L)
{
    const char *id = lua_tostring(L, lua_upvalueindex(1));
    luaL_checkany(L, 1);

    anim_entry_t *e = anim_find(id);
    if (!e)
        return luaL_error(L, "anim.backwards: '%s' not found", id);

    e->backwards = lua_toboolean(L, 1);
    return push_self(L, id);
}

static int l_anim_loop(lua_State *L)
{
    const char *id = lua_tostring(L, lua_upvalueindex(1));
    luaL_checkany(L, 1);

    anim_entry_t *e = anim_find(id);
    if (!e)
        return luaL_error(L, "anim.loop: '%s' not found", id);

    e->loop = lua_toboolean(L, 1);
    return push_self(L, id);
}

static int l_anim_stop(lua_State *L)
{
    const char *id = lua_tostring(L, lua_upvalueindex(1));

    anim_entry_t *e = anim_find(id);
    if (!e)
        return luaL_error(L, "anim.stop: '%s' not found", id);

    e->playing = 0;
    return push_self(L, id);
}

static int l_anim_play(lua_State *L)
{
    const char *id = lua_tostring(L, lua_upvalueindex(1));

    anim_entry_t *e = anim_find(id);
    if (!e)
        return luaL_error(L, "anim.play: '%s' not found", id);

    e->playing = 1;
    return push_self(L, id);
}

/* ------------------------------------------------------------------ */
/* anim() and fr() Lua functions                                      */
/* ------------------------------------------------------------------ */

static int l_anim(lua_State *L)
{
    if (lua_gfx_in_draw())
        return luaL_error(L, "anim: cannot be called inside _draw");

    const char *id = luaL_checkstring(L, 1);
    if (strlen(id) == 0)
        return luaL_error(L, "anim: id must not be empty");
    if (strlen(id) >= ANIM_ID_MAX)
        return luaL_error(L, "anim: id too long (max %d chars)",
                          ANIM_ID_MAX - 1);

    anim_entry_t *e = anim_find(id);
    if (!e) {
        e = anim_alloc(id);
        if (!e) return luaL_error(L, "anim: registry full");
    }

    char key[ANIM_KEY_MAX];
    make_key(id, key, sizeof(key));
    lua_getfield(L, LUA_REGISTRYINDEX, key);
    if (!lua_isnil(L, -1))
        return 1; /* return existing method table */
    lua_pop(L, 1);

    /* Build method table */
    static const struct {
        const char *name;
        lua_CFunction fn;
    } methods[] = {{"of", l_anim_of},
                   {"backwards", l_anim_backwards},
                   {"loop", l_anim_loop},
                   {"stop", l_anim_stop},
                   {"play", l_anim_play},
                   {NULL, NULL}};

    lua_newtable(L);
    for (int i = 0; methods[i].name; i++) {
        lua_pushstring(L, id);
        lua_pushcclosure(L, methods[i].fn, 1);
        lua_setfield(L, -2, methods[i].name);
    }

    /* Store in registry so subsequent anim(id) calls return the same table */
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, key);
    return 1;
}

static int l_aspr(lua_State *L)
{
    if (!lua_gfx_in_draw())
        return luaL_error(L, "aspr: not in draw context");

    const char *id = luaL_checkstring(L, 1);
    anim_entry_t *e = anim_find(id);
    if (!e)
        return luaL_error(L, "aspr: animation '%s' not registered", id);
    if (e->path[0] == '\0')
        return luaL_error(L, "aspr: animation '%s' not initialised", id);

    int x = (int)luaL_optinteger(L, 2, 0);
    int y = (int)luaL_optinteger(L, 3, 0);
    int fx = (int)luaL_optinteger(L, 4, 0);
    int fy = (int)luaL_optinteger(L, 5, 0);
    int fw = (int)luaL_optinteger(L, 6, 0);
    int fh = (int)luaL_optinteger(L, 7, 0);

    e->used_in_last_draw = 1;
    int frame = e->current_frame - 1;

    const char *err = NULL;
    if (spr_draw(e->path, frame, x, y, fx, fy, fw, fh,
                 gfx_fb(), GFX_W, GFX_H, &err) < 0)
        return luaL_error(L, "aspr: %s: %s", e->path,
                          err ? err : "unknown error");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void lua_anim_register(lua_State *L)
{
    lua_register(L, "anim", l_anim);
    lua_register(L, "aspr", l_aspr);
}

void lua_anim_clear_all(void)
{
    for (int i = 0; i < ANIM_MAX; i++)
        s_anims[i].id[0] = '\0';
}

cJSON *lua_anim_to_cjson(void)
{
    cJSON *obj = cJSON_CreateObject();
    for (int i = 0; i < ANIM_MAX; i++) {
        const anim_entry_t *e = &s_anims[i];
        if (e->id[0] == '\0' || e->path[0] == '\0') continue;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "path", e->path);
        cJSON_AddNumberToObject(entry, "n_frames", e->n_frames);
        cJSON_AddNumberToObject(entry, "current_frame", e->current_frame);
        cJSON_AddBoolToObject(entry, "backwards", e->backwards);
        cJSON_AddBoolToObject(entry, "playing", e->playing);
        cJSON_AddBoolToObject(entry, "loop", e->loop);
        cJSON_AddItemToObject(obj, e->id, entry);
    }
    return obj;
}

void lua_anim_restore(lua_State *L, const cJSON *obj)
{
    char key[ANIM_KEY_MAX];
    for (int i = 0; i < ANIM_MAX; i++) {
        if (s_anims[i].id[0] == '\0') continue;
        make_key(s_anims[i].id, key, sizeof(key));
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, key);
        s_anims[i].id[0] = '\0';
    }

    if (obj == NULL || cJSON_IsNull(obj) || !cJSON_IsObject(obj)) return;

    const cJSON *entry;
    cJSON_ArrayForEach(entry, obj)
    {
        const char *id = entry->string;
        if (!id) continue;
        cJSON *path_j = cJSON_GetObjectItemCaseSensitive(entry, "path");
        cJSON *n_j = cJSON_GetObjectItemCaseSensitive(entry, "n_frames");
        cJSON *cur_j = cJSON_GetObjectItemCaseSensitive(entry, "current_frame");
        cJSON *back_j = cJSON_GetObjectItemCaseSensitive(entry, "backwards");
        cJSON *play_j = cJSON_GetObjectItemCaseSensitive(entry, "playing");
        cJSON *loop_j = cJSON_GetObjectItemCaseSensitive(entry, "loop");

        if (!cJSON_IsString(path_j) || !cJSON_IsNumber(n_j) ||
            !cJSON_IsNumber(cur_j)) continue;

        anim_entry_t *e = anim_alloc(id);
        if (!e) continue;

        strncpy(e->path, path_j->valuestring, ANIM_PATH_MAX - 1);
        e->path[ANIM_PATH_MAX - 1] = '\0';
        e->n_frames = (int)n_j->valuedouble;
        e->current_frame = (int)cur_j->valuedouble;
        e->backwards = cJSON_IsTrue(back_j);
        e->playing = play_j ? cJSON_IsTrue(play_j) : 1;
        e->loop = cJSON_IsTrue(loop_j);
        e->used_in_last_draw = 0;
    }
}

void lua_anim_post_draw(lua_State *L)
{
    char key[ANIM_KEY_MAX];
    for (int i = 0; i < ANIM_MAX; i++) {
        anim_entry_t *e = &s_anims[i];
        if (e->id[0] == '\0') continue;

        if (!e->used_in_last_draw) {
            make_key(e->id, key, sizeof(key));
            lua_pushnil(L);
            lua_setfield(L, LUA_REGISTRYINDEX, key);
            e->id[0] = '\0';
            continue;
        }

        if (e->playing) {
            if (e->backwards) {
                if (e->current_frame == 1) {
                    if (e->loop)
                        e->current_frame = e->n_frames;
                    else
                        e->playing = 0;
                } else {
                    e->current_frame--;
                }
            } else {
                if (e->current_frame == e->n_frames) {
                    if (e->loop)
                        e->current_frame = 1;
                    else
                        e->playing = 0;
                } else {
                    e->current_frame++;
                }
            }
        }

        e->used_in_last_draw = 0;
    }
}
