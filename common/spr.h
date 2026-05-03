#ifndef GLOXIE_SPR_H
#define GLOXIE_SPR_H

#include <stdint.h>

/*
 * Load (or find cached) sprite at path; blit frame `frame` (0-based) to the
 * framebuffer at (x, y) using the given fragment.
 *
 * For a static PNG, frame must be 0. For APNG, frame is clamped to [0, n-1].
 * Multiple calls for the same path share decoded frame data.
 *
 * fx, fy   — top-left of fragment within the sprite canvas (0 = origin)
 * fw, fh   — fragment size; 0 means full canvas width/height
 * fb       — framebuffer (fb_w × fb_h uint32_t pixels, format 0x00RRGGBB)
 *
 * Returns 0 on success, -1 on error; if err_out is non-NULL it is set to a
 * static string.
 */
int spr_draw(const char *path, int frame, int x, int y, int fx, int fy, int fw,
             int fh, uint32_t *fb, int fb_w, int fb_h, const char **err_out);

/* Free all loaded sprite data. Call on Lua VM reset. */
void spr_clear_all(void);

#endif /* GLOXIE_SPR_H */
