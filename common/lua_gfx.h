#ifndef LUA_GFX_H
#define LUA_GFX_H

#include <stddef.h>
#include "../vendor/lua/lua.h"

/* Register graphics globals (cls, …) into L. Call before freeze_globals. */
void lua_gfx_register(lua_State *L);

/* Set (1) or clear (0) the draw context flag. Drawing globals (cls, spr, …)
   raise a Lua error when called outside a draw context. */
void lua_gfx_set_drawing(int v);

/* Return non-zero when currently inside a _draw() call. */
int lua_gfx_in_draw(void);

/* Resolve a path relative to the calling Lua script's directory.
   Absolute paths are used as-is. Result written to out[0..out_sz-1]. */
void lua_gfx_resolve_path(lua_State *L, const char *path, char *out,
                          size_t out_sz);

#endif /* LUA_GFX_H */
