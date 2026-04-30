#ifndef DISPLAY_H
#define DISPLAY_H

/* Open the SDL2 window and wire up the framebuffer.
   Non-fatal if no display is available (runs headless). */
void display_init(void);

/* Blit the current framebuffer to the window and drain SDL events.
   Returns 1 if the user closed the window, 0 otherwise. */
int display_poll(void);

void display_free(void);

#endif /* DISPLAY_H */
