#ifndef GLOXIE_LUA_ANIM_H
#define GLOXIE_LUA_ANIM_H

#include "../vendor/lua/lua.h"
#include "../vendor/cjson/cJSON.h"

/* Register anim() and fr() globals into L. */
void lua_anim_register(lua_State *L);

/* Clear the animation instance registry (C side only). Call on VM reset. */
void lua_anim_clear_all(void);

/* Advance all animation instances after a _draw() call: garbage-collect
   unused entries, advance frames for active ones, reset used flags. */
void lua_anim_post_draw(lua_State *L);

/* Serialize all active animation instances to a cJSON array.
   Caller owns the returned object and must cJSON_Delete it. */
cJSON *lua_anim_to_cjson(void);

/* Restore animation instances from a JSON array.  Clears all current
   instances (C side and Lua registry) first.  arr may be NULL or a JSON
   null (treated as empty).  Returns 0 on success, -1 if any entry is invalid. */
int lua_anim_restore(lua_State *L, const cJSON *arr);

#endif /* GLOXIE_LUA_ANIM_H */
