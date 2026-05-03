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

/* spr(path [, frame [, x [, y [, fx [, fy [, fw [, fh]]]]]]]])
   frame is a zero-based index into APNG frames; defaults to 0. */
static int l_spr(lua_State *L)
{
    const char *rel = luaL_checkstring(L, 1);
    int frame = (int)luaL_optinteger(L, 2, 0);
    int x = (int)luaL_optinteger(L, 3, 0);
    int y = (int)luaL_optinteger(L, 4, 0);
    int fx = (int)luaL_optinteger(L, 5, 0);
    int fy = (int)luaL_optinteger(L, 6, 0);
    int fw = (int)luaL_optinteger(L, 7, 0);
    int fh = (int)luaL_optinteger(L, 8, 0);

    char abs_path[1024];
    resolve_path(L, rel, abs_path, sizeof(abs_path));

    const char *err = NULL;
    if (spr_draw(abs_path, frame, x, y, fx, fy, fw, fh, gfx_fb(), GFX_W, GFX_H, &err) < 0)
        return luaL_error(L, "spr: %s: %s", abs_path, err ? err : "unknown error");
    return 0;
}

void lua_gfx_register(lua_State *L)
{
    spr_clear_all();
    lua_register(L, "cls", l_cls);
    lua_register(L, "spr", l_spr);
}
