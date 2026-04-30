#include "lua_gfx.h"
#include "gfx.h"
#include "../vendor/lua/lauxlib.h"

static int l_cls(lua_State *L)
{
    lua_Integer v = luaL_checkinteger(L, 1);
    gfx_cls((uint32_t)(v & 0xFFFFFF));
    return 0;
}

void lua_gfx_register(lua_State *L)
{
    lua_register(L, "cls", l_cls);
}
