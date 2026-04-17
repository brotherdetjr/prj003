#ifndef APP_H
#define APP_H

#include "../../core/world.h"
#include "../../vendor/mongoose/mongoose.h"
#include "../../vendor/lua/lua.h"

#define AUTOTICK 100U   /* virtual ms per timer tick; timer fires 10×/sec for ~10 FPS */

#define LUA_MAX_EVENTS  64U

typedef struct {
    char name[32];  /* empty string when slot is free; also the Lua global fn to call */
} lua_event_t;

typedef struct {
    world_t       world;
    char          instance_id[9];    /* "XXXXXXXX\0" */
    uint32_t      instance_id_raw;
    int           autotick;
    struct mg_mgr mgr;
    lua_State    *L;
    lua_event_t   lua_events[LUA_MAX_EVENTS];
    char          last_event_name[32]; /* name of the most recently fired Lua event */
    char          script_path[512];  /* path of the loaded Lua script */
} app_t;

#endif /* APP_H */
