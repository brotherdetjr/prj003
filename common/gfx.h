#ifndef GLOXIE_GFX_H
#define GLOXIE_GFX_H

#include <stddef.h>
#include <stdint.h>

#define GFX_W 368
#define GFX_H 448

/* Set the framebuffer (GFX_W * GFX_H uint32s, owned by the platform). */
void gfx_init(uint32_t *fb);

/* Return the current framebuffer pointer (for display refresh). */
uint32_t *gfx_fb(void);

/* Fill the entire screen with color (0x00RRGGBB). */
void gfx_cls(uint32_t color);

/* Encode current framebuffer as RGB PNG (uncompressed, no external deps).
   Returns a malloc'd buffer that the caller must free(); sets *len on success.
   Returns NULL on allocation failure. */
uint8_t *gfx_png(size_t *len);

#endif /* GLOXIE_GFX_H */
