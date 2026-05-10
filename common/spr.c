#include "spr.h"
#include "apng.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Content registry: path-keyed, shared across calls                  */
/* ------------------------------------------------------------------ */

typedef struct {
    char *path;
    uint8_t *frames; /* n_frames × canvas_w × canvas_h × 4 bytes */
    int n_frames;
    int canvas_w, canvas_h;
} spr_content_t;

static spr_content_t *s_content;
static int s_content_len, s_content_cap;

static int content_find(const char *path)
{
    for (int i = 0; i < s_content_len; i++)
        if (s_content[i].path && strcmp(s_content[i].path, path) == 0) return i;
    return -1;
}

static int content_alloc(void)
{
    if (s_content_len == s_content_cap) {
        int cap = s_content_cap ? s_content_cap * 2 : 8;
        spr_content_t *p = realloc(s_content, (size_t)cap * sizeof(*p));
        if (!p) return -1;
        s_content = p;
        s_content_cap = cap;
    }
    int idx = s_content_len++;
    memset(&s_content[idx], 0, sizeof(s_content[idx]));
    return idx;
}

/* ------------------------------------------------------------------ */
/* Internal pixel blitter                                             */
/* ------------------------------------------------------------------ */

static void do_blit(const spr_content_t *ct, int frame, int x, int y, int fx,
                    int fy, int fw, int fh, uint32_t *fb, int fb_w, int fb_h)
{
    fw = fw ? fw : ct->canvas_w;
    fh = fh ? fh : ct->canvas_h;

    if (fx < 0) {
        fw += fx;
        x -= fx;
        fx = 0;
    }
    if (fy < 0) {
        fh += fy;
        y -= fy;
        fy = 0;
    }
    if (fx + fw > ct->canvas_w) fw = ct->canvas_w - fx;
    if (fy + fh > ct->canvas_h) fh = ct->canvas_h - fy;
    if (fw <= 0 || fh <= 0) return;

    size_t frame_bytes = (size_t)ct->canvas_w * (size_t)ct->canvas_h * 4;
    const uint8_t *rgba = ct->frames + (size_t)frame * frame_bytes;

    for (int row = 0; row < fh; row++) {
        int sy = fy + row, dy = y + row;
        if (dy < 0 || dy >= fb_h) continue;
        for (int col = 0; col < fw; col++) {
            int sx = fx + col, dx = x + col;
            if (dx < 0 || dx >= fb_w) continue;
            const uint8_t *src =
                rgba + ((size_t)sy * (size_t)ct->canvas_w + (size_t)sx) * 4;
            uint8_t a = src[3];
            if (a == 0) continue;
            uint32_t *dst = &fb[(size_t)dy * (size_t)fb_w + (size_t)dx];
            if (a == 255) {
                *dst = ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
            } else {
                uint32_t dr = (*dst >> 16) & 0xFF;
                uint32_t dg = (*dst >> 8) & 0xFF;
                uint32_t db = *dst & 0xFF;
                *dst = (((src[0] * a + dr * (255 - a)) / 255) << 16) |
                       (((src[1] * a + dg * (255 - a)) / 255) << 8) |
                       ((src[2] * a + db * (255 - a)) / 255);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/* Load sprite into the content registry if not already present.
   Returns the content index on success, -1 on error. */
static int content_load(const char *path, const char **err_out)
{
    int cidx = content_find(path);
    if (cidx >= 0) return cidx;

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (err_out) *err_out = "cannot open file";
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    rewind(f);
    if (fsz <= 0) {
        fclose(f);
        if (err_out) *err_out = "empty file";
        return -1;
    }
    uint8_t *file_data = (uint8_t *)malloc((size_t)fsz);
    if (!file_data) {
        fclose(f);
        if (err_out) *err_out = "out of memory";
        return -1;
    }
    if (fread(file_data, 1, (size_t)fsz, f) != (size_t)fsz) {
        fclose(f);
        free(file_data);
        if (err_out) *err_out = "read error";
        return -1;
    }
    fclose(f);

    int n_frames = 0, w = 0, h = 0;
    uint8_t *frames = apng_load(file_data, (size_t)fsz, &n_frames, &w, &h);
    free(file_data);
    if (!frames) {
        if (err_out) *err_out = "decode error";
        return -1;
    }

    cidx = content_alloc();
    if (cidx < 0) {
        free(frames);
        if (err_out) *err_out = "out of memory";
        return -1;
    }
    s_content[cidx].path = strdup(path);
    s_content[cidx].frames = frames;
    s_content[cidx].n_frames = n_frames;
    s_content[cidx].canvas_w = w;
    s_content[cidx].canvas_h = h;
    return cidx;
}

int spr_frame_count(const char *path, const char **err_out)
{
    int cidx = content_load(path, err_out);
    if (cidx < 0) return -1;
    return s_content[cidx].n_frames;
}

int spr_draw(const char *path, int frame, int x, int y, int fx, int fy, int fw,
             int fh, uint32_t *fb, int fb_w, int fb_h, const char **err_out)
{
    int cidx = content_load(path, err_out);
    if (cidx < 0) return -1;

    spr_content_t *ct = &s_content[cidx];
    if (frame < 0) frame = 0;
    if (frame >= ct->n_frames) frame = ct->n_frames - 1;
    do_blit(ct, frame, x, y, fx, fy, fw, fh, fb, fb_w, fb_h);
    return 0;
}

void spr_clear_all(void)
{
    for (int i = 0; i < s_content_len; i++) {
        free(s_content[i].path);
        free(s_content[i].frames);
    }
    free(s_content);
    s_content = NULL;
    s_content_len = 0;
    s_content_cap = 0;
}
