/* Drawing into the frame: flat fills, smooth (anti-aliased) shapes from signed distances, and the
 * baked Poppins fonts. Shapes are only worked out inside their bounding boxes. */
#include <stdint.h>
#include <string.h>

#include "tt.h"

static uint32_t frame_px[VIEW_W * VIEW_H];
uint32_t *fb = frame_px;

static inline uint32_t blend(uint32_t dst, uint32_t src, unsigned a) {
    /* a 0-256 */
    uint32_t rb = ((src & 0xff00ffu) * a + (dst & 0xff00ffu) * (256 - a)) >> 8 & 0xff00ffu;
    uint32_t g = ((src & 0x00ff00u) * a + (dst & 0x00ff00u) * (256 - a)) >> 8 & 0x00ff00u;
    return rb | g;
}

static inline float fsqrt(float x) { return __builtin_sqrtf(x < 0 ? 0 : x); }
static inline float fmaxf_(float a, float b) { return a > b ? a : b; }
static inline float fminf_(float a, float b) { return a < b ? a : b; }
static inline float fabsf_(float a) { return a < 0 ? -a : a; }

/* atan2 to about 0.005 rad, enough to cut an arc */
static float fatan2(float y, float x) {
    float ax = fabsf_(x), ay = fabsf_(y);
    if (ax == 0 && ay == 0) return 0;
    float a = fminf_(ax, ay) / fmaxf_(ax, ay), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}

void fill(uint32_t c) {
    for (int i = 0; i < VIEW_W * VIEW_H; ++i) fb[i] = c;
}

void rect(int x, int y, int w, int h, uint32_t c, int alpha) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > VIEW_W ? VIEW_W : x + w, y1 = y + h > VIEW_H ? VIEW_H : y + h;
    if (alpha <= 0) return;
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t *o = fb + yy * VIEW_W;
        for (int xx = x0; xx < x1; ++xx) o[xx] = alpha >= 256 ? c : blend(o[xx], c, (unsigned)alpha);
    }
}

void darken(int alpha) { rect(0, 0, VIEW_W, VIEW_H, 0, alpha); }

/* The shapes: SDF(px, py) is the signed distance from the pixel's centre to the edge (inside
 * negative), and the pixel is covered by 0.5 - d of it. */
#define SHAPE(X0, Y0, X1, Y1, SDF)                                                                    \
    do {                                                                                              \
        if (alpha <= 0) return;                                                                       \
        int x0_ = (int)(X0) - 1, y0_ = (int)(Y0) - 1, x1_ = (int)(X1) + 2, y1_ = (int)(Y1) + 2;       \
        if (x0_ < 0) x0_ = 0;                                                                         \
        if (y0_ < 0) y0_ = 0;                                                                         \
        if (x1_ > VIEW_W) x1_ = VIEW_W;                                                               \
        if (y1_ > VIEW_H) y1_ = VIEW_H;                                                               \
        for (int yy = y0_; yy < y1_; ++yy) {                                                          \
            uint32_t *o = fb + yy * VIEW_W;                                                           \
            float py = (float)yy + 0.5f;                                                              \
            for (int xx = x0_; xx < x1_; ++xx) {                                                      \
                float px = (float)xx + 0.5f;                                                          \
                float d = (SDF);                                                                      \
                if (d >= 0.5f) continue;                                                              \
                int a = d <= -0.5f ? alpha : (int)((0.5f - d) * (float)alpha);                        \
                o[xx] = a >= 256 ? c : blend(o[xx], c, (unsigned)a);                                 \
            }                                                                                         \
        }                                                                                             \
    } while (0)

static inline float sd_rrect(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = fabsf_(px - cx) - hw + r, qy = fabsf_(py - cy) - hh + r;
    float ox = fmaxf_(qx, 0), oy = fmaxf_(qy, 0);
    return fsqrt(ox * ox + oy * oy) + fminf_(fmaxf_(qx, qy), 0) - r;
}

void rrect(float x, float y, float w, float h, float r, uint32_t c, int alpha) {
    float cx = x + w / 2, cy = y + h / 2;
    SHAPE(x, y, x + w, y + h, sd_rrect(px, py, cx, cy, w / 2, h / 2, r));
}

void rrect_line(float x, float y, float w, float h, float r, float t, uint32_t c, int alpha) {
    float cx = x + w / 2, cy = y + h / 2;
    SHAPE(x, y, x + w, y + h, fabsf_(sd_rrect(px, py, cx, cy, w / 2 - t / 2, h / 2 - t / 2, r - t / 2)) - t / 2);
}

static inline float sd_segment(float px, float py, float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0, l = dx * dx + dy * dy;
    float t = l > 0 ? ((px - x0) * dx + (py - y0) * dy) / l : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    float ex = px - x0 - t * dx, ey = py - y0 - t * dy;
    return fsqrt(ex * ex + ey * ey);
}

void capsule(float x0, float y0, float x1, float y1, float r, uint32_t c, int alpha) {
    SHAPE(fminf_(x0, x1) - r, fminf_(y0, y1) - r, fmaxf_(x0, x1) + r, fmaxf_(y0, y1) + r,
          sd_segment(px, py, x0, y0, x1, y1) - r);
}

void disc(float cx, float cy, float r, uint32_t c, int alpha) {
    SHAPE(cx - r, cy - r, cx + r, cy + r, fsqrt((px - cx) * (px - cx) + (py - cy) * (py - cy)) - r);
}

/* a ring of radius r and thickness t, from angle a0 for sweep radians (clockwise on screen: y is
 * down), with round ends */
static inline float sd_arc(float px, float py, float cx, float cy, float r, float t, float a0, float sweep,
                           float ex0, float ey0, float ex1, float ey1) {
    float dx = px - cx, dy = py - cy;
    float ring = fabsf_(fsqrt(dx * dx + dy * dy) - r) - t / 2;
    if (sweep >= 6.2831f) return ring;
    float a = fatan2(dy, dx) - a0;
    while (a < 0) a += 6.2831853f;
    while (a >= 6.2831853f) a -= 6.2831853f;
    if (a <= sweep) return ring;
    float d0 = fsqrt((px - ex0) * (px - ex0) + (py - ey0) * (py - ey0));
    float d1 = fsqrt((px - ex1) * (px - ex1) + (py - ey1) * (py - ey1));
    return fminf_(d0, d1) - t / 2;
}

static float fsin(float a) { /* -2pi..2pi is plenty here */
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    float s = a * a;
    return a * (1 - s / 6 * (1 - s / 20 * (1 - s / 42 * (1 - s / 72 * (1 - s / 110)))));
}
static float fcos(float a) { return fsin(a + 1.5707963f); }

void arc(float cx, float cy, float r, float t, float a0, float sweep, uint32_t c, int alpha) {
    if (sweep <= 0) return;
    float ex0 = cx + fcos(a0) * r, ey0 = cy + fsin(a0) * r;
    float ex1 = cx + fcos(a0 + sweep) * r, ey1 = cy + fsin(a0 + sweep) * r;
    float R = r + t / 2;
    SHAPE(cx - R, cy - R, cx + R, cy + R, sd_arc(px, py, cx, cy, r, t, a0, sweep, ex0, ey0, ex1, ey1));
}

/* ---------------------------------------------------------------------------------------------
 * Text */

static const Glyph *glyph(const Font *f, char ch) {
    if (ch < 32 || ch > 126) ch = '?';
    return &f->g[ch - 32];
}

int text_w(const Font *f, const char *s) {
    int w = 0;
    for (; *s; ++s) w += glyph(f, *s)->adv;
    return w;
}

void text(const Font *f, const char *s, int x, int y, uint32_t c, int alpha) {
    int top = y - f->ascent;
    for (; *s; ++s) {
        const Glyph *g = glyph(f, *s);
        int gx = x + g->lsb;
        for (int r = 0; r < f->h; ++r) {
            int yy = top + r;
            if (yy < 0 || yy >= VIEW_H) continue;
            const uint8_t *in = f->px + r * f->aw + g->x;
            uint32_t *o = fb + yy * VIEW_W;
            for (int k = 0; k < g->w; ++k) {
                int xx = gx + k;
                if (!in[k] || xx < 0 || xx >= VIEW_W) continue;
                unsigned a = (unsigned)(in[k] + 1) * (unsigned)alpha >> 8;
                o[xx] = a >= 256 ? c : blend(o[xx], c, a);
            }
        }
        x += g->adv;
    }
}

void text_c(const Font *f, const char *s, int cx, int y, uint32_t c, int alpha) {
    text(f, s, cx - text_w(f, s) / 2, y, c, alpha);
}
