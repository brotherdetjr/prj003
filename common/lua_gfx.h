#ifndef LUA_GFX_H
#define LUA_GFX_H

#include "../vendor/lua/lua.h"

/* Register graphics globals (cls, …) into L. Call before freeze_globals. */
void lua_gfx_register(lua_State *L);

#endif /* LUA_GFX_H */
