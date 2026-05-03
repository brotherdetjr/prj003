#include "spr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vendor/stb/stb_image.h"

/* ------------------------------------------------------------------ */
/* CRC-32 (same polynomial as gfx.c — duplicated to keep spr.c       */
/* independent of gfx.c's internal linkage)                           */
/* ------------------------------------------------------------------ */

static uint32_t s_crc_tab[256];
static int s_crc_ready;

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        s_crc_tab[n] = c;
    }
    s_crc_ready = 1;
}

static uint32_t crc32_buf(const uint8_t *data, size_t len)
{
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        c = s_crc_tab[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

/* ------------------------------------------------------------------ */
/* Minimal growable byte buffer                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *buf;
    size_t len, cap;
} buf_t;

static int buf_append(buf_t *b, const uint8_t *data, size_t n)
{
    if (b->len + n > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 4096;
        while (cap < b->len + n)
            cap *= 2;
        uint8_t *p = realloc(b->buf, cap);
        if (!p) return 0;
        b->buf = p;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, data, n);
    b->len += n;
    return 1;
}

static void buf_free(buf_t *b)
{
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

/* ------------------------------------------------------------------ */
/* Minimal PNG builder (wraps raw IDAT data from APNG frames)         */
/* ------------------------------------------------------------------ */

static void u32be(buf_t *b, uint32_t v)
{
    uint8_t tmp[4] = {(v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF};
    buf_append(b, tmp, 4);
}

static void png_chunk(buf_t *out, const char *type, const uint8_t *data, uint32_t dlen)
{
    u32be(out, dlen);
    size_t crc_start = out->len;
    buf_append(out, (const uint8_t *)type, 4);
    if (dlen) buf_append(out, data, dlen);
    u32be(out, crc32_buf(out->buf + crc_start, 4 + dlen));
}

static uint8_t *build_png(int w, int h, const uint8_t *idat_data, size_t idat_len,
                          size_t *out_len)
{
    if (!s_crc_ready) crc_init();

    buf_t png = {NULL, 0, 0};
    static const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    buf_append(&png, sig, 8);

    uint8_t ihdr[13] = {
        (w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF,
        (h >> 24) & 0xFF, (h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF,
        8, 6, 0, 0, 0}; /* bit_depth=8, color_type=RGBA */
    png_chunk(&png, "IHDR", ihdr, 13);
    png_chunk(&png, "IDAT", idat_data, (uint32_t)idat_len);
    png_chunk(&png, "IEND", NULL, 0);

    if (!png.buf) return NULL;
    *out_len = png.len;
    return png.buf;
}

/* ------------------------------------------------------------------ */
/* APNG helpers                                                        */
/* ------------------------------------------------------------------ */

static uint32_t read_u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

#define DISPOSE_OP_NONE 0
#define DISPOSE_OP_BACKGROUND 1
#define DISPOSE_OP_PREVIOUS 2
#define BLEND_OP_SOURCE 0
#define BLEND_OP_OVER 1

typedef struct {
    uint32_t x, y, w, h;
    uint8_t dispose_op;
    uint8_t blend_op;
    buf_t idat; /* zlib payload — sequence numbers stripped for fdAT */
} apng_frame_t;

static void blend_over(uint8_t *dst, const uint8_t *src)
{
    uint32_t sa = src[3];
    if (sa == 255) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 255;
    } else if (sa > 0) {
        uint32_t da = dst[3];
        uint32_t out_a = sa + da * (255 - sa) / 255;
        if (out_a == 0) {
            dst[3] = 0;
            return;
        }
        dst[0] = (uint8_t)((src[0] * sa + dst[0] * da * (255 - sa) / 255) / out_a);
        dst[1] = (uint8_t)((src[1] * sa + dst[1] * da * (255 - sa) / 255) / out_a);
        dst[2] = (uint8_t)((src[2] * sa + dst[2] * da * (255 - sa) / 255) / out_a);
        dst[3] = (uint8_t)out_a;
    }
}

/* ------------------------------------------------------------------ */
/* PNG / APNG loader: returns flat RGBA canvas frames or NULL         */
/* ------------------------------------------------------------------ */

static uint8_t *load_frames(const uint8_t *file_data, size_t file_len,
                            int *out_n, int *out_w, int *out_h)
{
    if (!s_crc_ready) crc_init();

    static const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (file_len < 8 || memcmp(file_data, sig, 8) != 0) return NULL;

    const uint8_t *p = file_data + 8;
    const uint8_t *end = file_data + file_len;

    if (p + 25 > end) return NULL;
    if (read_u32be(p) < 13 || memcmp(p + 4, "IHDR", 4) != 0) return NULL;
    int canvas_w = (int)read_u32be(p + 8);
    int canvas_h = (int)read_u32be(p + 12);

    int n_frames = 0;
    int is_apng = 0;
    const uint8_t *scan = p;
    while (scan + 8 <= end) {
        uint32_t clen = read_u32be(scan);
        if (clen > (size_t)(end - scan - 12)) break;
        if (memcmp(scan + 4, "acTL", 4) == 0 && clen >= 8) {
            is_apng = 1;
            n_frames = (int)read_u32be(scan + 8);
        }
        scan += 12 + clen;
    }

    if (!is_apng || n_frames <= 1) {
        int w, h, ch;
        uint8_t *rgba = stbi_load_from_memory(file_data, (int)file_len, &w, &h, &ch, 4);
        if (!rgba) return NULL;
        *out_n = 1;
        *out_w = w;
        *out_h = h;
        return rgba;
    }

    apng_frame_t *frames = (apng_frame_t *)calloc((size_t)n_frames, sizeof(apng_frame_t));
    if (!frames) return NULL;
    for (int i = 0; i < n_frames; i++) {
        frames[i].w = (uint32_t)canvas_w;
        frames[i].h = (uint32_t)canvas_h;
    }

    int frame_idx = -1;
    int seen_idat = 0;
    int idat_frame = 0;

    scan = p;
    while (scan + 8 <= end) {
        uint32_t clen = read_u32be(scan);
        const uint8_t *type = scan + 4;
        const uint8_t *cdata = scan + 8;
        if (clen > (size_t)(end - scan - 12)) break;

        if (memcmp(type, "fcTL", 4) == 0 && clen >= 26) {
            frame_idx++;
            if (frame_idx >= n_frames) break;
            frames[frame_idx].w = read_u32be(cdata + 4);
            frames[frame_idx].h = read_u32be(cdata + 8);
            frames[frame_idx].x = read_u32be(cdata + 12);
            frames[frame_idx].y = read_u32be(cdata + 16);
            frames[frame_idx].dispose_op = cdata[22];
            frames[frame_idx].blend_op = cdata[23];
            idat_frame = 0;
        } else if (memcmp(type, "IDAT", 4) == 0) {
            if (!seen_idat) {
                seen_idat = 1;
                if (frame_idx < 0) {
                    frame_idx = 0;
                    idat_frame = 1;
                } else {
                    idat_frame = 1;
                }
            }
            if (frame_idx >= 0 && frame_idx < n_frames)
                buf_append(&frames[frame_idx].idat, cdata, clen);
        } else if (memcmp(type, "fdAT", 4) == 0 && clen >= 4) {
            if (frame_idx >= 0 && frame_idx < n_frames && !idat_frame)
                buf_append(&frames[frame_idx].idat, cdata + 4, clen - 4);
        }

        scan += 12 + clen;
    }

    size_t canvas_px = (size_t)canvas_w * (size_t)canvas_h;
    size_t frame_bytes = canvas_px * 4;
    uint8_t *result = (uint8_t *)calloc((size_t)n_frames, frame_bytes);
    uint8_t *canvas = (uint8_t *)calloc(canvas_px, 4);
    uint8_t *prev = (uint8_t *)calloc(canvas_px, 4);
    if (!result || !canvas || !prev) {
        free(result);
        free(canvas);
        free(prev);
        result = NULL;
        goto cleanup;
    }

    for (int i = 0; i < n_frames; i++) {
        apng_frame_t *f = &frames[i];

        if (i > 0) {
            apng_frame_t *pf = &frames[i - 1];
            if (pf->dispose_op == DISPOSE_OP_BACKGROUND) {
                for (uint32_t row = 0; row < pf->h; row++)
                    memset(canvas + ((pf->y + row) * (uint32_t)canvas_w + pf->x) * 4,
                           0, pf->w * 4);
            } else if (pf->dispose_op == DISPOSE_OP_PREVIOUS) {
                memcpy(canvas, prev, canvas_px * 4);
            }
        }
        memcpy(prev, canvas, canvas_px * 4);

        if (f->idat.buf && f->idat.len > 0) {
            size_t png_len;
            uint8_t *mini =
                build_png((int)f->w, (int)f->h, f->idat.buf, f->idat.len, &png_len);
            if (mini) {
                int fw, fh, ch;
                uint8_t *px =
                    stbi_load_from_memory(mini, (int)png_len, &fw, &fh, &ch, 4);
                free(mini);
                if (px) {
                    for (uint32_t row = 0; row < f->h; row++) {
                        uint8_t *dst =
                            canvas + ((f->y + row) * (uint32_t)canvas_w + f->x) * 4;
                        const uint8_t *src = px + row * f->w * 4;
                        if (f->blend_op == BLEND_OP_OVER) {
                            for (uint32_t col = 0; col < f->w; col++)
                                blend_over(dst + col * 4, src + col * 4);
                        } else {
                            memcpy(dst, src, f->w * 4);
                        }
                    }
                    stbi_image_free(px);
                }
            }
        }
        memcpy(result + (size_t)i * frame_bytes, canvas, frame_bytes);
    }

    free(canvas);
    free(prev);
    *out_n = n_frames;
    *out_w = canvas_w;
    *out_h = canvas_h;

cleanup:
    for (int i = 0; i < n_frames; i++)
        buf_free(&frames[i].idat);
    free(frames);
    return result;
}

/* ------------------------------------------------------------------ */
/* Content registry: path-keyed, shared across calls                 */
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
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int spr_draw(const char *path, int frame, int x, int y, int fx, int fy, int fw,
             int fh, uint32_t *fb, int fb_w, int fb_h, const char **err_out)
{
    int cidx = content_find(path);
    if (cidx < 0) {
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
        uint8_t *frames = load_frames(file_data, (size_t)fsz, &n_frames, &w, &h);
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
    }

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
