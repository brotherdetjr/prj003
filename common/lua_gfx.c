#include "lua_gfx.h"
#include "gfx.h"
#include "spr.h"
#include "../vendor/lua/lauxlib.h"
#include <string.h>

static int l_cls(lua_State *L)
{
    lua_Integer v = luaL_checkinteger(L, 1);
    gfx_cls((uint32_t)(v & 0xFFFFFF));
    return 0;
}

/* ------------------------------------------------------------------ */
/* spr() Lua binding                                                  */
/* ------------------------------------------------------------------ */

/* Resolve a path relative to the calling Lua script's directory.
   Walks the call stack to find the first source file with a real path. */
static void resolve_path(lua_State *L, const char *path,
                         char *out, size_t out_sz)
{
    if (path[0] == '/') {
        /* Absolute path — use as-is */
        snprintf(out, out_sz, "%s", path);
        return;
    }
    lua_Debug ar;
    for (int level = 1; lua_getstack(L, level, &ar); level++) {
        lua_getinfo(L, "S", &ar);
        const char *src = ar.source;
        if (src && src[0] == '@') {
            /* src is "@/path/to/script.lua" — strip '@' and take directory */
            src++; /* skip '@' */
            const char *slash = strrchr(src, '/');
            if (slash) {
                size_t dir_len = (size_t)(slash - src);
                snprintf(out, out_sz, "%.*s/%s", (int)dir_len, src, path);
                return;
            }
        }
    }
    /* Fallback: use path as-is */
    snprintf(out, out_sz, "%s", path);
}

/* Handle method closures — each captures state_idx as upvalue 1 */
static int l_spr_stop(lua_State *L)
{
    int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
    spr_stop(idx);
    return 0;
}

static int l_spr_play(lua_State *L)
{
    int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
    spr_play(idx);
    return 0;
}

static int l_spr_reset(lua_State *L)
{
    int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
    spr_reset(idx);
    return 0;
}

static int l_spr_reverse(lua_State *L)
{
    int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
    spr_reverse(idx);
    return 0;
}

static int l_spr_loop(lua_State *L)
{
    int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
    int enabled = lua_toboolean(L, 1);
    spr_loop(idx, enabled);
    return 0;
}

static int l_spr_set_frame(lua_State *L)
{
    int idx = (int)lua_tointeger(L, lua_upvalueindex(1));
    int frame = (int)luaL_checkinteger(L, 1);
    spr_set_frame(idx, frame);
    return 0;
}

/* spr(path [, x [, y [, fx [, fy [, fw [, fh]]]]]]])
   Returns a handle table with playback-control methods. */
static int l_spr(lua_State *L)
{
    const char *rel = luaL_checkstring(L, 1);
    int x = (int)luaL_optinteger(L, 2, 0);
    int y = (int)luaL_optinteger(L, 3, 0);
    int fx = (int)luaL_optinteger(L, 4, 0);
    int fy = (int)luaL_optinteger(L, 5, 0);
    int fw = (int)luaL_optinteger(L, 6, 0);
    int fh = (int)luaL_optinteger(L, 7, 0);

    char abs_path[1024];
    resolve_path(L, rel, abs_path, sizeof(abs_path));

    const char *err = NULL;
    int idx = spr_new(abs_path, x, y, fx, fy, fw, fh, gfx_fb(), GFX_W, GFX_H, &err);
    if (idx < 0)
        return luaL_error(L, "spr: %s: %s", abs_path, err ? err : "unknown error");

    /* Build handle table: { stop, play, reset, reverse, loop, set_frame } */
    lua_createtable(L, 0, 6);
    static const struct {
        const char *name;
        lua_CFunction fn;
    } methods[] = {
        {"stop", l_spr_stop},
        {"play", l_spr_play},
        {"reset", l_spr_reset},
        {"reverse", l_spr_reverse},
        {"loop", l_spr_loop},
        {"set_frame", l_spr_set_frame},
        {NULL, NULL},
    };
    for (int i = 0; methods[i].name; i++) {
        lua_pushinteger(L, (lua_Integer)idx);
        lua_pushcclosure(L, methods[i].fn, 1);
        lua_setfield(L, -2, methods[i].name);
    }
    return 1;
}

void lua_gfx_register(lua_State *L)
{
    spr_clear_all();
    lua_register(L, "cls", l_cls);
    lua_register(L, "spr", l_spr);
}
