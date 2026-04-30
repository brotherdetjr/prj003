#ifndef DISPLAY_H
#define DISPLAY_H

/* Wire up the framebuffer and, unless headless, open the SDL2 window.
   Non-fatal if the window cannot be created (falls back to headless). */
void display_init(int headless);

/* Blit the current framebuffer to the window and drain SDL events.
   Returns 1 if the user closed the window, 0 otherwise. */
int display_poll(void);

void display_free(void);

#endif /* DISPLAY_H */
