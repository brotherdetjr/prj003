#include "gfx.h"
#include <stdlib.h>
#include <string.h>

static uint32_t *s_fb;

void gfx_init(uint32_t *fb) { s_fb = fb; }
uint32_t *gfx_fb(void) { return s_fb; }

void gfx_cls(uint32_t color)
{
    for (int i = 0; i < GFX_W * GFX_H; i++)
        s_fb[i] = color;
}

/* ------------------------------------------------------------------ */
/* Minimal PNG encoder (uncompressed deflate, no external deps)       */
/* ------------------------------------------------------------------ */

static uint32_t s_crc_tab[256];
static int s_crc_ready;

static void crc_build(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        s_crc_tab[n] = c;
    }
    s_crc_ready = 1;
}

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        c = s_crc_tab[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

static uint32_t adler32(const uint8_t *data, size_t len)
{
    uint32_t s1 = 1, s2 = 0;
    while (len > 0) {
        size_t n = len > 5552 ? 5552 : len;
        len -= n;
        while (n--) {
            s1 += *data++;
            s2 += s1;
        }
        s1 %= 65521;
        s2 %= 65521;
    }
    return (s2 << 16) | s1;
}

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
} pbuf_t;

static int pb_reserve(pbuf_t *pb, size_t need)
{
    if (pb->len + need <= pb->cap) return 1;
    size_t cap = pb->cap ? pb->cap * 2 : 65536;
    while (cap < pb->len + need)
        cap *= 2;
    uint8_t *p = realloc(pb->buf, cap);
    if (!p) return 0;
    pb->buf = p;
    pb->cap = cap;
    return 1;
}

static void pb_u8(pbuf_t *pb, uint8_t v)
{
    if (pb_reserve(pb, 1)) pb->buf[pb->len++] = v;
}

static void pb_u16le(pbuf_t *pb, uint16_t v)
{
    if (!pb_reserve(pb, 2)) return;
    pb->buf[pb->len++] = v & 0xFF;
    pb->buf[pb->len++] = (v >> 8) & 0xFF;
}

static void pb_u32be(pbuf_t *pb, uint32_t v)
{
    if (!pb_reserve(pb, 4)) return;
    pb->buf[pb->len++] = (v >> 24) & 0xFF;
    pb->buf[pb->len++] = (v >> 16) & 0xFF;
    pb->buf[pb->len++] = (v >> 8) & 0xFF;
    pb->buf[pb->len++] = v & 0xFF;
}

static void pb_append(pbuf_t *pb, const uint8_t *data, size_t n)
{
    if (!pb_reserve(pb, n)) return;
    memcpy(pb->buf + pb->len, data, n);
    pb->len += n;
}

static void png_chunk(pbuf_t *out, const char *type, const uint8_t *data, uint32_t dlen)
{
    pb_u32be(out, dlen);
    size_t crc_off = out->len;
    pb_append(out, (const uint8_t *)type, 4);
    if (dlen) pb_append(out, data, dlen);
    pb_u32be(out, crc32(out->buf + crc_off, 4 + dlen));
}

uint8_t *gfx_png(size_t *out_len)
{
    if (!s_crc_ready) crc_build();

    /* Filtered scanlines: 1 filter byte (0 = None) + 3 RGB bytes per pixel. */
    size_t row_bytes = 1 + GFX_W * 3;
    uint8_t *raw = malloc(GFX_H * row_bytes);
    if (!raw) return NULL;

    for (int y = 0; y < GFX_H; y++) {
        uint8_t *row = raw + (size_t)y * row_bytes;
        row[0] = 0;
        for (int x = 0; x < GFX_W; x++) {
            uint32_t px = s_fb[(unsigned)y * GFX_W + (unsigned)x];
            row[1 + x * 3 + 0] = (px >> 16) & 0xFF;
            row[1 + x * 3 + 1] = (px >> 8) & 0xFF;
            row[1 + x * 3 + 2] = px & 0xFF;
        }
    }

    /* IDAT: zlib header + deflate stored blocks (one per row) + Adler-32.
       CMF=0x78 (deflate, 32K window), FLG=0x01: (0x7801 % 31 == 0). */
    pbuf_t idat = {NULL, 0, 0};
    pb_u8(&idat, 0x78);
    pb_u8(&idat, 0x01);
    for (int y = 0; y < GFX_H; y++) {
        uint16_t len = (uint16_t)row_bytes;
        pb_u8(&idat, y == GFX_H - 1 ? 0x01 : 0x00); /* BFINAL | BTYPE=stored */
        pb_u16le(&idat, len);
        pb_u16le(&idat, (uint16_t)~len);
        pb_append(&idat, raw + (size_t)y * row_bytes, row_bytes);
    }
    pb_u32be(&idat, adler32(raw, GFX_H * row_bytes));
    free(raw);

    pbuf_t png = {NULL, 0, 0};
    static const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    pb_append(&png, sig, 8);

    uint8_t ihdr[13] = {
        0, 0, (GFX_W >> 8) & 0xFF, GFX_W & 0xFF,
        0, 0, (GFX_H >> 8) & 0xFF, GFX_H & 0xFF,
        8, 2, 0, 0, 0}; /* bit_depth=8, color_type=RGB, no interlace */
    png_chunk(&png, "IHDR", ihdr, 13);
    png_chunk(&png, "IDAT", idat.buf, (uint32_t)idat.len);
    free(idat.buf);
    png_chunk(&png, "IEND", NULL, 0);

    if (!png.buf) return NULL;
    *out_len = png.len;
    return png.buf;
}
