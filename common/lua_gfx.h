#ifndef LUA_GFX_H
#define LUA_GFX_H

#include "../vendor/lua/lua.h"

/* Register graphics globals (cls, …) into L. Call before freeze_globals. */
void lua_gfx_register(lua_State *L);

/* Set (1) or clear (0) the draw context flag. Drawing globals (cls, spr, …)
   raise a Lua error when called outside a draw context. */
void lua_gfx_set_drawing(int v);

#endif /* LUA_GFX_H */
