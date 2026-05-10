#ifndef GLOXIE_LUA_ANIM_H
#define GLOXIE_LUA_ANIM_H

#include "../vendor/lua/lua.h"

/* Register anim() and fr() globals into L. */
void lua_anim_register(lua_State *L);

/* Clear the animation instance registry (C side only). Call on VM reset. */
void lua_anim_clear_all(void);

/* Advance all animation instances after a _draw() call: garbage-collect
   unused entries, advance frames for active ones, reset used flags. */
void lua_anim_post_draw(lua_State *L);

#endif /* GLOXIE_LUA_ANIM_H */
