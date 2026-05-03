#ifndef GLOXIE_SPR_H
#define GLOXIE_SPR_H

#include <stdint.h>

/*
 * Create a new animation instance for the sprite at path, draw its first
 * frame at (x, y) using the given fragment, and return a state index >= 0.
 * Returns -1 on error; if err_out is non-NULL it is set to a static string.
 *
 * fx, fy   — top-left of fragment within the sprite canvas (0 = origin)
 * fw, fh   — fragment size; 0 means full canvas width/height
 * fb       — framebuffer (fb_w × fb_h uint32_t pixels, format 0x00RRGGBB)
 *
 * Default state: current_frame=0, playing=false, direction=+1, loop=false.
 * Multiple instances for the same path share decoded frame data.
 */
int spr_new(const char *path, int x, int y, int fx, int fy, int fw, int fh,
            uint32_t *fb, int fb_w, int fb_h, const char **err_out);

/*
 * Advance all playing animation instances by the ticks elapsed since the
 * previous call, redrawing each that moves to a new frame.
 * Call once per advance_time step with the new now_tick value.
 */
void spr_tick_advance(uint64_t now_tick, uint32_t *fb, int fb_w, int fb_h);

/* Playback controls — all silently ignore invalid idx. */
void spr_stop(int idx);
void spr_play(int idx);
void spr_reset(int idx);
void spr_reverse(int idx);
void spr_loop(int idx, int enabled);
void spr_set_frame(int idx, int frame);

/* Free all loaded sprite data. Call on Lua VM reset. */
void spr_clear_all(void);

#endif /* GLOXIE_SPR_H */
