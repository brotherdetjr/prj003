#ifndef EMU_H
#define EMU_H

#include <stdint.h>

typedef struct {
    uint64_t start_ts;      /* initial virtual time; 0 = use wall clock */
    int      auto_tick;     /* 1 = advance every WORLD_TICK_S, 0 = manual */
    char     filename[256]; /* file to load at startup; empty = none */
} emu_config_t;

void emu_run(emu_config_t *cfg);

#endif /* EMU_H */
