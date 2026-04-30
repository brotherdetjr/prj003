#include "display.h"
#include "../../common/gfx.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static uint32_t s_fb[GFX_W * GFX_H];

static SDL_Window *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture *s_tex;
static int s_active;

void display_init(void)
{
    gfx_init(s_fb);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "display: SDL_Init failed (%s), running headless\n",
                SDL_GetError());
        return;
    }
    s_win = SDL_CreateWindow("Gloxie", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, GFX_W, GFX_H,
                             SDL_WINDOW_SHOWN);
    if (!s_win) {
        fprintf(stderr, "display: SDL_CreateWindow failed (%s)\n", SDL_GetError());
        SDL_Quit();
        return;
    }
    s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
    if (!s_ren) {
        SDL_DestroyWindow(s_win);
        s_win = NULL;
        SDL_Quit();
        return;
    }
    s_tex = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
    if (!s_tex) {
        SDL_DestroyRenderer(s_ren);
        SDL_DestroyWindow(s_win);
        s_ren = NULL;
        s_win = NULL;
        SDL_Quit();
        return;
    }
    s_active = 1;
}

int display_poll(void)
{
    if (!s_active) return 0;
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
        if (ev.type == SDL_QUIT) return 1;
    SDL_UpdateTexture(s_tex, NULL, s_fb, GFX_W * (int)sizeof(uint32_t));
    SDL_RenderCopy(s_ren, s_tex, NULL, NULL);
    SDL_RenderPresent(s_ren);
    return 0;
}

void display_free(void)
{
    if (!s_active) return;
    SDL_DestroyTexture(s_tex);
    SDL_DestroyRenderer(s_ren);
    SDL_DestroyWindow(s_win);
    SDL_Quit();
}
