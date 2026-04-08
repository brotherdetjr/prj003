#ifndef APP_H
#define APP_H

#include "../../core/world.h"
#include "../../vendor/mongoose/mongoose.h"

typedef struct {
    world_t       world;
    char          instance_id[9];  /* "XXXXXXXX\0" */
    uint32_t      instance_id_raw;
    int           autotick;
    struct mg_mgr mgr;
} app_t;

#endif /* APP_H */
