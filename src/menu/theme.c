#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "memory_log.h"
#include "theme.h"
#include "theme_parse.h"
#include "safe_mode.h"
#include "builtin_themes.h"
#include "card_themes.h"
#include "menu_options.h"

static theme_t theme;
static surface_t background;
static bool initialized = false;     /* settings loaded */
static bool background_built = false;
static bool from_sd = false;
static bool suspended = false;        /* memory lent to another screen */

/* The background is built at 1/BG_SCALE of the screen size and stretched when
   drawn. It is kept in full 32-bit color (300 KB instead of the 600 KB a
   full-size 16-bit one took), and the graphics chip dithers it down to the
   16-bit screen at full resolution, so the dither grain stays fine. */
#define BG_SCALE    (2)

/* ---------- defaults ---------- */

static void theme_set_defaults (theme_t *t) {
    memset(t, 0, sizeof(*t));
    strcpy(t->name, "Midnight Gradient");
    t->text = RGBA32(0xE9, 0xED, 0xF2, 0xFF);
    t->text_dim = RGBA32(0xA7, 0xB0, 0xBB, 0xFF);
    t->accent = RGBA32(0xF2, 0xB1, 0x34, 0xFF);
    t->panel = RGBA32(0x1B, 0x20, 0x28, 0xFF);
    /* Same as the stock menu, so an old theme.ini looks unchanged. */
    t->border = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
    t->highlight = RGBA32(0x7F, 0x7F, 0x7F, 0xFF);
    t->tab_active = RGBA32(0x6F, 0x6F, 0x6F, 0xFF);
    t->tab_inactive = RGBA32(0x3F, 0x3F, 0x3F, 0xFF);
    t->tab_active_border = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
    t->tab_inactive_border = RGBA32(0x5F, 0x5F, 0x5F, 0xFF);
    t->bg_type = THEME_BG_GRADIENT;
    t->direction = THEME_DIR_VERTICAL;
    t->color1 = RGBA32(0x1B, 0x2A, 0x4A, 0xFF);
    t->color2 = RGBA32(0x0B, 0x0E, 0x14, 0xFF);
    t->color3 = RGBA32(0x3A, 0x2F, 0x6B, 0xFF);
    t->use_color3 = false;
    t->dither = true;
    t->drift = THEME_DRIFT_USUAL;
    t->pattern = THEME_PATTERN_NONE;
    t->pattern_color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
    t->pattern_size = 16;
    t->pattern_opacity = 10;
    for (int i = 0; i < FEATURE_COUNT; i++) {
        t->features[i] = FEATURE_UNSET;
    }
}

void theme_defaults (theme_t *t) {
    theme_set_defaults(t);
}

/* ---------- background generation ---------- */

typedef struct { float r, g, b; } rgbf_t;

static rgbf_t to_f (color_t c) {
    return (rgbf_t) { c.r, c.g, c.b };
}

static rgbf_t lerp (rgbf_t a, rgbf_t b, float t) {
    return (rgbf_t) { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

static rgbf_t sky_color (const theme_t *t, float u, float v);   /* the sky, further down */

static float gradient_pos (const theme_t *t, int x, int y, int w, int h) {
    switch (t->direction) {
        case THEME_DIR_HORIZONTAL:
            return (float) x / (float) (w - 1);
        case THEME_DIR_DIAGONAL:
            return ((float) x * w + (float) y * h) / ((float) w * w + (float) h * h);
        case THEME_DIR_RADIAL: {
            float dx = x - w / 2.0f;
            float dy = y - h / 2.0f;
            float d = sqrtf(dx * dx + dy * dy) / (sqrtf((float) w * w + (float) h * h) / 2.0f);
            return d > 1.0f ? 1.0f : d;
        }
        case THEME_DIR_VERTICAL:
        default:
            return (float) y / (float) (h - 1);
    }
}

/* The gradient's color at a screen position. */
static rgbf_t gradient_color (const theme_t *t, int x, int y, int w, int h) {
    float p = gradient_pos(t, x, y, w, h);
    p = (p < 0.0f) ? 0.0f : ((p > 1.0f) ? 1.0f : p);
    rgbf_t c1 = to_f(t->color1);
    rgbf_t c2 = to_f(t->color2);
    if (t->use_color3) {
        rgbf_t c3 = to_f(t->color3);
        return (p < 0.5f) ? lerp(c1, c3, p * 2.0f) : lerp(c3, c2, (p - 0.5f) * 2.0f);
    }
    return lerp(c1, c2, p);
}

static bool pattern_hit (const theme_t *t, int x, int y) {
    int s = t->pattern_size;
    switch (t->pattern) {
        case THEME_PATTERN_STRIPES:
            return (y % (2 * s)) < s;
        case THEME_PATTERN_DIAGONAL:
            return ((x + y) % (2 * s)) < s;
        case THEME_PATTERN_CHECKER:
            return (((x / s) + (y / s)) % 2) == 0;
        case THEME_PATTERN_DOTS: {
            float r = s * 0.18f;
            if (r < 1.5f) r = 1.5f;
            float dx = (x % s) + 0.5f - s / 2.0f;
            float dy = (y % s) + 0.5f - s / 2.0f;
            return dx * dx + dy * dy <= r * r;
        }
        case THEME_PATTERN_GRID:
            return (x % s) < 2 || (y % s) < 2;
        case THEME_PATTERN_SCANLINES: {
            int gap = (s + 1) / 2;
            if (gap < 4) gap = 4;
            return (y % gap) < 2;
        }
        case THEME_PATTERN_NONE:
        default:
            return false;
    }
}

/* ---------- the ocean background ---------- */

/* Water seen from straight above: a net of pale foam lines over blue, the
   same size all over the screen. The lines are the borders between
   scattered points' territories (which gives the rounded, uneven cells of
   light on water), bent a little so they aren't straight. Tried out on a PC
   first with the same sums. It began as water running away to a horizon
   under a strip of sky; the user asked for it flat (2026-10-09), which is
   also less work and has no speckled distance. */
#define OCEAN_CELL      (160.0f)    /* screen pixels across one cell: four cells span the screen, so the repeat doesn't show.
                                       Smaller cells (128) were tried: the background is built at half size, and finer lines only looked blocky. */
#define OCEAN_LINE      (0.06f)     /* foam is solid nearer a border than this... */
#define OCEAN_LINE_SOFT (0.11f)     /* ...and gone beyond this */

/* Working the foam out for every pixel of the screen took about 1.4 s (a
   plain gradient takes 0.3 s). So it is worked out once for a small square
   of water, OCEAN_PERIOD cells each way, that repeats; the screen's pixels
   then only look their place up in that square. The square holds "how far
   from a border" rather than a picture of the lines, and four neighboring
   values are blended for each pixel, so the lines stay crisp however much
   the square is stretched near the bottom of the screen. */
#define OCEAN_TILE      (64)        /* the square's size in values each way: 4 KB, small enough to stay in the console's fast memory */
#define OCEAN_PERIOD    (4)         /* cells across it before it repeats */
#define OCEAN_STORE     (1020.0f)   /* a border distance of 0.25 fills the byte */
static uint8_t *ocean_tile = NULL;

/* A fixed scatter of points: the same "random" spot for a cell every time,
   and the same again OCEAN_PERIOD cells on, so the square's edges meet. */
static uint32_t ocean_scatter (int ix, int iz) {
    ix = ((ix % OCEAN_PERIOD) + OCEAN_PERIOD) % OCEAN_PERIOD;
    iz = ((iz % OCEAN_PERIOD) + OCEAN_PERIOD) % OCEAN_PERIOD;
    uint32_t h = ((uint32_t) ix * 374761393u) + ((uint32_t) iz * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* How much nearer the nearest point is than the next nearest: 0 on a border. */
static float ocean_border (float x, float z) {
    int ix = (int) x;
    int iz = (int) z;
    if (x < ix) ix--;       /* round down, also below zero */
    if (z < iz) iz--;
    float nearest = 9.0f, second = 9.0f;

    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint32_t h = ocean_scatter(ix + dx, iz + dz);
            float px = (ix + dx) + ((h & 0xFFFF) * (1.0f / 65536.0f));
            float pz = (iz + dz) + (((h >> 16) & 0xFFFF) * (1.0f / 65536.0f));
            float d = ((px - x) * (px - x)) + ((pz - z) * (pz - z));
            if (d < nearest) {
                second = nearest;
                nearest = d;
            } else if (d < second) {
                second = d;
            }
        }
    }
    return sqrtf(second) - sqrtf(nearest);
}

/* The bend in the lines needs two sines for every pixel, and working a sine
   out properly is slow on this console, so they are looked up in a small
   table instead. The bend is gentle; the table's steps don't show. */
#define OCEAN_WAVE_STEPS    (64)
static float ocean_wave_table[OCEAN_WAVE_STEPS];

static float ocean_wave (float angle) {
    int step = (int) (angle * (OCEAN_WAVE_STEPS / 6.2831853f));
    return ocean_wave_table[step & (OCEAN_WAVE_STEPS - 1)];
}

/* Work out the square of water and the sine table. False if out of memory. */
static bool ocean_prepare (void) {
    ocean_tile = malloc(OCEAN_TILE * OCEAN_TILE);
    if (!ocean_tile) {
        return false;
    }
    for (int tz = 0; tz < OCEAN_TILE; tz++) {
        for (int tx = 0; tx < OCEAN_TILE; tx++) {
            float border = ocean_border((tx * (float) OCEAN_PERIOD) / OCEAN_TILE, (tz * (float) OCEAN_PERIOD) / OCEAN_TILE);
            int stored = (int) (border * OCEAN_STORE);
            ocean_tile[(tz * OCEAN_TILE) + tx] = (stored > 255) ? 255 : stored;
        }
    }
    for (int i = 0; i < OCEAN_WAVE_STEPS; i++) {
        ocean_wave_table[i] = sinf((i * 6.2831853f) / OCEAN_WAVE_STEPS);
    }
    return true;
}

static void ocean_finish (void) {
    free(ocean_tile);
    ocean_tile = NULL;
}

/* The border distance at a spot on the water, blended from the square's
   four nearest values. */
static float ocean_lookup (float side, float away) {
    /* Moved a long way along first, so the sums never go below zero. */
    float tu = (side + 4096.0f) * ((float) OCEAN_TILE / OCEAN_PERIOD);
    float tz = (away + 4096.0f) * ((float) OCEAN_TILE / OCEAN_PERIOD);
    int iu = (int) tu;
    int iz = (int) tz;
    float fu = tu - iu;
    float fz = tz - iz;
    int u0 = iu % OCEAN_TILE;
    int z0 = iz % OCEAN_TILE;
    int u1 = (u0 + 1 == OCEAN_TILE) ? 0 : (u0 + 1);
    int z1 = (z0 + 1 == OCEAN_TILE) ? 0 : (z0 + 1);

    float a = ocean_tile[(z0 * OCEAN_TILE) + u0];
    float b = ocean_tile[(z0 * OCEAN_TILE) + u1];
    float c = ocean_tile[(z1 * OCEAN_TILE) + u0];
    float d = ocean_tile[(z1 * OCEAN_TILE) + u1];
    float upper = a + ((b - a) * fu);
    float lower = c + ((d - c) * fu);
    return (upper + ((lower - upper) * fz)) * (1.0f / OCEAN_STORE);
}

static rgbf_t ocean_color (int x, int y, rgbf_t water, rgbf_t foam) {
    float side = x * (1.0f / OCEAN_CELL);
    float away = y * (1.0f / OCEAN_CELL);

    float border = ocean_lookup(side + (0.22f * ocean_wave((away * 2.3f) + (side * 1.1f))), away + (0.22f * ocean_wave((side * 1.9f) - (away * 0.7f))));
    if (border > OCEAN_LINE_SOFT) {
        return water;
    }
    float line = (border < OCEAN_LINE) ? 1.0f : ((OCEAN_LINE_SOFT - border) / (OCEAN_LINE_SOFT - OCEAN_LINE));
    return lerp(water, foam, line);
}

static uint32_t to_byte (float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (uint32_t) lroundf(v);
}

static void theme_build_background (const theme_t *t) {
    uint64_t build_started = get_ticks_ms();
    int w = display_get_width();
    int h = display_get_height();
    int bw = w / BG_SCALE;
    int bh = h / BG_SCALE;

    background = surface_alloc(FMT_RGBA32, bw, bh);

    /* Build each row in cached RAM, then copy it out in one go. */
    uint32_t *row = malloc(bw * sizeof(uint32_t));
    if (!row || !background.buffer) {
        free(row);
        return;
    }

    rgbf_t c1 = to_f(t->color1);
    rgbf_t c2 = to_f(t->color2);
    rgbf_t pc = to_f(t->pattern_color);
    float pa = t->pattern_opacity / 100.0f;

    /* Image backgrounds aren't decoded yet: fall back to color1. */
    theme_bg_type_t type = (t->bg_type == THEME_BG_IMAGE) ? THEME_BG_SOLID : t->bg_type;
    if (type == THEME_BG_OCEAN && !ocean_prepare()) {
        type = THEME_BG_SOLID;      /* no memory to spare for it: plain water */
    }

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            /* Gradient and pattern are worked out in screen positions, so
               pattern sizes in theme.ini still mean screen pixels. */
            int x = bx * BG_SCALE;
            int y = by * BG_SCALE;
            rgbf_t c;
            if (type == THEME_BG_GRADIENT) {
                c = gradient_color(t, x, y, w, h);
            } else if (type == THEME_BG_OCEAN) {
                c = ocean_color(x, y, c1, c2);
            } else if (type == THEME_BG_SKY) {
                c = sky_color(t, (x + 0.5f) / w, (y + 0.5f) / h);
            } else {
                c = c1;
            }

            if (pa > 0.0f && pattern_hit(t, x, y)) {
                c = lerp(c, pc, pa);
            }

            row[bx] = (to_byte(c.r) << 24) | (to_byte(c.g) << 16) | (to_byte(c.b) << 8) | 0xFF;
        }
        memcpy((uint8_t *) background.buffer + by * background.stride, row, bw * sizeof(uint32_t));
    }

    free(row);
    ocean_finish();
    debugf("theme: background \"%s\" built in %d ms\n", t->name, (int) (get_ticks_ms() - build_started));
}

/* ---------- sky ---------- */

/* A night sky (type = sky): dark above, a glow at the bottom (color2 to
   color1, most of the change near the bottom), with soft clouds in color3
   that thin out toward the glow. The clouds are value noise in four
   octaves, stretched sideways into wisps, and repeat exactly once across
   the screen so they can drift without a seam. Checked first on the PC with
   a Python copy of these sums (the owner's Ocarina of Time File Select-style
   theme, 2026-11-26). */
static uint32_t sky_hash (uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t n = x * 374761393u + y * 668265263u + seed * 2246822519u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n = n ^ (n >> 16);
    return n & 0xFFFF;
}

static float sky_smooth (float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float sky_noise (float u, float v, int cells_x, int cells_y, uint32_t seed) {
    float gx = u * cells_x, gy = v * cells_y;
    int ix = (int) floorf(gx), iy = (int) floorf(gy);
    float fx = sky_smooth(gx - ix), fy = sky_smooth(gy - iy);
    int x0 = ((ix % cells_x) + cells_x) % cells_x, x1 = (x0 + 1) % cells_x;
    int y0 = ((iy % cells_y) + cells_y) % cells_y, y1 = (y0 + 1) % cells_y;
    float a = sky_hash(x0, y0, seed) / 65535.0f, b = sky_hash(x1, y0, seed) / 65535.0f;
    float c = sky_hash(x0, y1, seed) / 65535.0f, d = sky_hash(x1, y1, seed) / 65535.0f;
    float top = a + (b - a) * fx, bottom = c + (d - c) * fx;
    return top + (bottom - top) * fy;
}

/* u, v: 0-1 across and down the screen (u repeats every 1). */
static rgbf_t sky_color (const theme_t *t, float u, float v) {
    rgbf_t glow = to_f(t->color1), top = to_f(t->color2);
    rgbf_t cloud = t->use_color3 ? to_f(t->color3) : lerp(top, (rgbf_t) { 255.0f, 255.0f, 255.0f }, 0.4f);
    rgbf_t base = lerp(top, glow, powf(v, 2.6f));
    float n = 0.0f, amp = 0.5f;
    int cx = 3, cy = 6;
    for (int o = 0; o < 4; o++) {
        n += amp * sky_noise(u, v, cx, cy, 7 + o);
        amp *= 0.5f;
        cx *= 2;
        cy *= 2;
    }
    n /= 0.9375f;
    float a = (n - 0.42f) / 0.38f;
    a = (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f : a;
    a = sky_smooth(a) * 0.45f * (1.0f - 0.75f * v);
    return lerp(base, cloud, a);
}

/* ---------- the live background ---------- */

/* Gradients and plain colors are not kept as a picture (300 KB) but drawn
   afresh each frame as a few shaded triangles, which the graphics chip
   blends smoothly and dithers like the picture. A pattern is a small tile
   (at most 64 x 64, 4 KB) repeated over it; the chip mixes the pattern
   color in where the tile says. Ocean, and patterns whose tile can't be
   repeated by the chip (a size that isn't a power of two, or too large),
   keep the stored picture. */
#define LIVE_TILE_MAX   (64)

static bool live = false;               /* drawn each frame, no stored picture */
static surface_t pattern_tile;          /* the pattern's strength per pixel, or none */

/* A round gradient (and a diagonal one with a middle color) can't be drawn
   exactly with one or two boxes; made of many small ones it cost about 14 ms
   a frame (an orange radial theme, 2026-11-10). It is drawn instead from a
   tiny picture of itself, one value per 8 x 8 pixels (80 x 60, 19 KB),
   stretched over the screen with smoothing: a smooth gradient looks the
   same. With a pattern it keeps the full stored picture. */
#define SMALL_SCALE     (8)
static surface_t gradient_small;

/* The pattern repeats every this many pixels both ways (0 when it can't be
   drawn live). */
static int pattern_period (const theme_t *t) {
    int s = t->pattern_size;
    int p;
    switch (t->pattern) {
        case THEME_PATTERN_STRIPES:
        case THEME_PATTERN_DIAGONAL:
        case THEME_PATTERN_CHECKER:
            p = 2 * s;
            break;
        case THEME_PATTERN_DOTS:
        case THEME_PATTERN_GRID:
            p = s;
            break;
        case THEME_PATTERN_SCANLINES:
            p = (s + 1) / 2;
            if (p < 4) p = 4;
            break;
        default:
            return 0;
    }
    return (p > 0 && p <= LIVE_TILE_MAX) ? p : 0;
}

/* The chip repeats a tile only if its size is a power of two. A pattern that
   repeats every p pixels otherwise (Grape's 24 px checker: 48) is drawn into
   the next power of two up (64) and the tile is stretched back to p pixels
   as it is drawn (tile_scale), so it repeats every p pixels on screen. Edges
   that fall on whole pixels stay sharp (the checker's squares). */
static int tile_size (int p) {
    int t = 1;
    while (t < p) {
        t <<= 1;
    }
    return t;
}
static float tile_scale = 1.0f;     /* tile texels per screen pixel */

/* The tile for a period: the power of two below it when that still gives the
   pattern exactly (Grape's 48 px checker from a 32 px tile: 1 KB, not 4),
   else the one above. */
static int tile_pick (const theme_t *t, int p) {
    int up = tile_size(p);
    int down = (up == p) ? p : up / 2;
    if (down == p || down < 2) {
        return up;
    }
    float k = (float) down / p;
    for (int y = 0; y < p; y++) {
        int ty = (int) ((y + 0.5f) * k);
        int py = (int) ((ty + 0.5f) / k);
        for (int x = 0; x < p; x++) {
            int tx = (int) ((x + 0.5f) * k);
            if (pattern_hit(t, (int) ((tx + 0.5f) / k), py) != pattern_hit(t, x, y)) {
                return up;
            }
        }
    }
    return down;
}

/* A round or diagonal gradient with a pattern: both from small pictures in
   one pass of the chip (two textures). The gradient picture shares the chip's
   4 KB with the tile, so it is coarser than gradient_small (16-bit, a value
   every 16-32 px) and scaled from the tile's coordinates by a power of two. */
static surface_t gradient_coarse;
static int coarse_log = 0;          /* the gradient's texels are the tile's divided by 2^coarse_log */

static bool has_pattern (const theme_t *t) {
    return t->pattern != THEME_PATTERN_NONE && t->pattern_opacity > 0;
}

static bool needs_small (const theme_t *t) {
    return t->bg_type == THEME_BG_SKY
        || (t->bg_type == THEME_BG_GRADIENT
            && (t->direction == THEME_DIR_RADIAL || (t->direction == THEME_DIR_DIAGONAL && t->use_color3)));
}

static bool live_possible (const theme_t *t) {
    if (t->bg_type == THEME_BG_OCEAN) {
        return false;
    }
    if (t->bg_type == THEME_BG_SKY) {
        return true;    /* the sky draws no pattern */
    }
    return !has_pattern(t) || pattern_period(t) > 0;
}

static void small_prepare (const theme_t *t) {
    int w = display_get_width();
    int h = display_get_height();
    int sw = w / SMALL_SCALE;
    int sh = h / SMALL_SCALE;
    bool sky = (t->bg_type == THEME_BG_SKY);
    gradient_small = surface_alloc(FMT_RGBA32, sw + (sky ? 1 : 0), sh);    /* the sky's last column repeats its first, for the drift */
    if (!gradient_small.buffer) {
        debugf("theme: no memory for the small gradient, plain color instead\n");
        return;
    }
    for (int y = 0; y < sh; y++) {
        uint32_t *out = (uint32_t *) ((uint8_t *) gradient_small.buffer + y * gradient_small.stride);
        for (int x = 0; x < gradient_small.width; x++) {
            /* Each value is the color at the middle of the 8 x 8 pixels it stands for. */
            rgbf_t c = sky ? sky_color(t, (x + 0.5f) / sw, (y + 0.5f) / sh)
                           : gradient_color(t, x * SMALL_SCALE + SMALL_SCALE / 2, y * SMALL_SCALE + SMALL_SCALE / 2, w, h);
            out[x] = (to_byte(c.r) << 24) | (to_byte(c.g) << 16) | (to_byte(c.b) << 8) | 0xFF;
        }
    }
}

static bool tile_prepare (const theme_t *t) {
    int p = pattern_period(t);
    int size = tile_pick(t, p);
    tile_scale = (float) size / p;
    pattern_tile = surface_alloc(FMT_I8, size, size);
    if (!pattern_tile.buffer) {
        debugf("theme: no memory for the pattern tile, pattern left out\n");
        return false;
    }
    uint8_t strength = to_byte(t->pattern_opacity * 255.0f / 100.0f);
    for (int y = 0; y < size; y++) {
        uint8_t *out = (uint8_t *) pattern_tile.buffer + y * pattern_tile.stride;
        int py = (int) ((y + 0.5f) / tile_scale);    /* the screen pixel this texel stands for */
        for (int x = 0; x < size; x++) {
            out[x] = pattern_hit(t, (int) ((x + 0.5f) / tile_scale), py) ? strength : 0;
        }
    }
    return true;
}

static void coarse_prepare (const theme_t *t) {
    int w = display_get_width();
    int h = display_get_height();
    int tile_bytes = pattern_tile.stride * pattern_tile.height;
    for (coarse_log = 0; coarse_log <= 10; coarse_log++) {
        float per_px = tile_scale / (float) (1 << coarse_log);     /* gradient texels per screen pixel */
        if (1.0f / per_px < 16.0f) {
            continue;
        }
        int gw = (int) ceilf(w * per_px) + 1;
        int gh = (int) ceilf(h * per_px) + 1;
        int stride = ((gw * 2) + 7) & ~7;
        if (stride * gh + tile_bytes > 4096) {
            continue;
        }
        gradient_coarse = surface_alloc(FMT_RGBA16, gw, gh);
        if (!gradient_coarse.buffer) {
            return;
        }
        for (int y = 0; y < gh; y++) {
            uint16_t *out = (uint16_t *) ((uint8_t *) gradient_coarse.buffer + y * gradient_coarse.stride);
            for (int x = 0; x < gw; x++) {
                rgbf_t c = gradient_color(t, (int) ((x + 0.5f) / per_px), (int) ((y + 0.5f) / per_px), w, h);
                out[x] = (uint16_t) (((to_byte(c.r) >> 3) << 11) | ((to_byte(c.g) >> 3) << 6) | ((to_byte(c.b) >> 3) << 1) | 1);
            }
        }
        return;
    }
}

static void live_prepare (const theme_t *t) {
    live = true;
    tile_scale = 1.0f;
    if (needs_small(t) && has_pattern(t)) {
        if (tile_prepare(t)) {
            coarse_prepare(t);
        }
        if (gradient_coarse.buffer) {
            debugf("theme: background \"%s\" drawn live, a %dx%d gradient and a %dx%d pattern tile in one pass\n", t->name,
                   gradient_coarse.width, gradient_coarse.height, pattern_tile.width, pattern_tile.height);
            return;
        }
        if (pattern_tile.buffer) {      /* no room: the gradient alone */
            surface_free(&pattern_tile);
            memset(&pattern_tile, 0, sizeof(pattern_tile));
        }
    }
    if (needs_small(t)) {
        small_prepare(t);
        debugf("theme: background \"%s\" drawn live from a %dx%d picture\n", t->name, gradient_small.width, gradient_small.height);
        return;
    }
    if (!has_pattern(t)) {
        debugf("theme: background \"%s\" drawn live\n", t->name);
        return;
    }
    if (tile_prepare(t)) {
        debugf("theme: background \"%s\" drawn live, pattern every %d px from a %dx%d tile\n", t->name,
               pattern_period(t), pattern_tile.width, pattern_tile.height);
    }
}

static void live_forget (void) {
    if (gradient_small.buffer) {
        surface_free(&gradient_small);
        memset(&gradient_small, 0, sizeof(gradient_small));
    }
    if (pattern_tile.buffer) {
        surface_free(&pattern_tile);
        memset(&pattern_tile, 0, sizeof(pattern_tile));
    }
    if (gradient_coarse.buffer) {
        surface_free(&gradient_coarse);
        memset(&gradient_coarse, 0, sizeof(gradient_coarse));
    }
    live = false;
}

/* One corner of a triangle: position, color (0-1), pattern position. */
/* Drawing the background over something, part see-through (theme_background_fade_part()); 255 = solid, as usual. */
static int over_alpha = 255;

static void live_vertex (float *v, int x, int y, int w, int h, float bright) {
    rgbf_t c = (theme.bg_type == THEME_BG_GRADIENT) ? gradient_color(&theme, x, y, w, h) : to_f(theme.color1);
    float k = bright / (255.0f * 255.0f);   /* the chip wants colors 0-1; the theme's are 0-255 */
    v[0] = x;
    v[1] = y;
    v[2] = c.r * k;
    v[3] = c.g * k;
    v[4] = c.b * k;
    v[5] = over_alpha / 255.0f;
    v[6] = x * tile_scale;
    v[7] = y * tile_scale;
    v[8] = 1.0f;
}

/* The sky: its small picture twice side by side, moved left a little more
   every frame, so the clouds drift; the picture's extra last column matches
   its first, so the join is smooth. Speed `drift` crosses one screen width in
   SKY_DRIFT_MS / drift (3, the usual: two minutes). The place is added up
   from the time passed, so a change of speed (Edit Theme) carries on from
   where the clouds are instead of jumping. */
#define SKY_DRIFT_MS    (360000)

static void sky_draw (int x0, int y0, int x1, int y1, int bright, int w, int h) {
    static float shift = 0.0f;
    static uint32_t shift_ms = 0;
    uint32_t now = get_ticks_ms();
    shift += (float) (uint32_t) (now - shift_ms) * w * theme.drift / SKY_DRIFT_MS;
    shift_ms = now;
    if (shift >= w) {
        shift = fmodf(shift, (float) w);
    }
    int t0 = y0 / SMALL_SCALE;
    int t1 = (y1 + SMALL_SCALE - 1) / SMALL_SCALE;
    if (t1 > gradient_small.height) t1 = gradient_small.height;
    if (t1 <= t0) {
        return;
    }
    rdpq_set_mode_standard();
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
    if (bright < 255 || over_alpha < 255) {
        rdpq_set_prim_color(RGBA32(bright, bright, bright, over_alpha));
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        if (over_alpha < 255) {
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
    } else {
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    }
    rdpq_set_scissor(x0, y0, x1, y1);
    for (int copy = 0; copy < 2; copy++) {
        float left = copy * w - shift;     /* where this copy's first column starts */
        float right = left + gradient_small.width * SMALL_SCALE;
        if (right <= x0 || left >= x1) {
            continue;
        }
        /* Only the columns over the part, one either side for the smoothing. */
        int s0 = (int) floorf((x0 - left) / SMALL_SCALE) - 1;
        int s1 = (int) ceilf((x1 - left) / SMALL_SCALE) + 1;
        if (s0 < 0) s0 = 0;
        if (s1 > gradient_small.width) s1 = gradient_small.width;
        if (s1 <= s0) {
            continue;
        }
        rdpq_tex_blit(&gradient_small, left + s0 * SMALL_SCALE, t0 * SMALL_SCALE, &(rdpq_blitparms_t) {
            .s0 = s0,
            .t0 = t0,
            .width = s1 - s0,
            .height = t1 - t0,
            .scale_x = SMALL_SCALE,
            .scale_y = SMALL_SCALE,
            .filtering = true,
        });
    }
    rdpq_set_scissor(0, 0, w, h);
}

/* Draws the part x0..x1, y0..y1 of the background, `bright` 0-255. */
static void live_draw (int x0, int y0, int x1, int y1, int bright) {
    int w = display_get_width();
    int h = display_get_height();
    bool tiled = (pattern_tile.buffer != NULL);
    bool gradient = (theme.bg_type == THEME_BG_GRADIENT);

    if (gradient_small.buffer && theme.bg_type == THEME_BG_SKY) {
        sky_draw(x0, y0, x1, y1, bright, w, h);
        return;
    }
    if (gradient_coarse.buffer) {
        /* The gradient (TEX0, smoothed) darkened by ENV, then the pattern color
           mixed in by the tile (TEX1), in the chip's two-cycle mode. */
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
        rdpq_mode_combiner(RDPQ_COMBINER2((TEX0, 0, ENV, 0), (0, 0, 0, ENV), (PRIM, COMBINED, TEX1, COMBINED), (0, 0, 0, COMBINED)));
        rdpq_set_env_color(RGBA32(bright, bright, bright, over_alpha));
        rdpq_set_prim_color(RGBA32(theme.pattern_color.r * bright / 255, theme.pattern_color.g * bright / 255, theme.pattern_color.b * bright / 255, 0xFF));
        if (over_alpha < 255) {
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        rdpq_tex_multi_begin();
        rdpq_tex_upload(TILE0, &gradient_coarse, &(rdpq_texparms_t) { .s.scale_log = coarse_log, .t.scale_log = coarse_log });
        rdpq_tex_upload(TILE1, &pattern_tile, &(rdpq_texparms_t) { .s.repeats = REPEAT_INFINITE, .t.repeats = REPEAT_INFINITE });
        rdpq_tex_multi_end();
        rdpq_texture_rectangle_scaled(TILE0, x0, y0, x1, y1,
            x0 * tile_scale, y0 * tile_scale, x1 * tile_scale, y1 * tile_scale);
        return;
    }
    if (gradient_small.buffer) {
        /* The tiny picture, stretched and smoothed; only the texels the part
           needs, cut to the part by the scissor. */
        int s0 = x0 / SMALL_SCALE;
        int t0 = y0 / SMALL_SCALE;
        int s1 = (x1 + SMALL_SCALE - 1) / SMALL_SCALE;
        int t1 = (y1 + SMALL_SCALE - 1) / SMALL_SCALE;
        if (s1 > gradient_small.width) s1 = gradient_small.width;
        if (t1 > gradient_small.height) t1 = gradient_small.height;
        if (s1 <= s0 || t1 <= t0) {
            return;
        }
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
        if (bright < 255 || over_alpha < 255) {
            rdpq_set_prim_color(RGBA32(bright, bright, bright, over_alpha));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
            if (over_alpha < 255) {
                rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            }
        } else {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        }
        bool part = (x0 > 0 || y0 > 0 || x1 < w || y1 < h);
        if (part) {
            rdpq_set_scissor(x0, y0, x1, y1);
        }
        rdpq_tex_blit(&gradient_small, s0 * SMALL_SCALE, t0 * SMALL_SCALE, &(rdpq_blitparms_t) {
            .s0 = s0,
            .t0 = t0,
            .width = s1 - s0,
            .height = t1 - t0,
            .scale_x = SMALL_SCALE,
            .scale_y = SMALL_SCALE,
            .filtering = true,
        });
        if (part) {
            rdpq_set_scissor(0, 0, w, h);
        }
        return;
    }

    if (!gradient && !tiled) {
        color_t plain = RGBA32(theme.color1.r * bright / 255, theme.color1.g * bright / 255, theme.color1.b * bright / 255, over_alpha);
        if (over_alpha < 255) {
            rdpq_set_mode_standard();
            rdpq_set_prim_color(plain);
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        } else {
            rdpq_set_mode_fill(plain);
        }
        rdpq_fill_rectangle(x0, y0, x1, y1);
        return;
    }

    /* Straight gradients come out exact from a single box, split where the
       middle color sits; a round one is made of small boxes. */
    int cols = 1, rows = 1;
    if (gradient) {
        switch (theme.direction) {
            case THEME_DIR_HORIZONTAL: cols = theme.use_color3 ? 2 : 1; break;
            case THEME_DIR_DIAGONAL:   if (theme.use_color3) { cols = 8; rows = 6; } break;
            case THEME_DIR_RADIAL:     cols = 16; rows = 12; break;
            case THEME_DIR_VERTICAL:
            default:                   rows = theme.use_color3 ? 2 : 1; break;
        }
    }

    rdpq_set_mode_standard();
    rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
    if (tiled) {
        /* The pattern color mixed into the gradient by the tile's strength. */
        rdpq_mode_combiner(RDPQ_COMBINER1((PRIM, SHADE, TEX0, SHADE), (0, 0, 0, SHADE)));
        rdpq_set_prim_color(RGBA32(theme.pattern_color.r * bright / 255, theme.pattern_color.g * bright / 255, theme.pattern_color.b * bright / 255, 0xFF));
        rdpq_tex_upload(TILE0, &pattern_tile, &(rdpq_texparms_t) {
            .s.repeats = REPEAT_INFINITE,
            .t.repeats = REPEAT_INFINITE,
        });
    } else {
        rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    }
    if (over_alpha < 255) {
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);   /* the shade's alpha: part see-through */
    }
    const rdpq_trifmt_t *format = tiled ? &TRIFMT_SHADE_TEX : &TRIFMT_SHADE;

    for (int row = 0; row < rows; row++) {
        int top = (y0 > row * h / rows) ? y0 : row * h / rows;
        int bottom = (y1 < (row + 1) * h / rows) ? y1 : (row + 1) * h / rows;
        if (bottom <= top) {
            continue;
        }
        for (int col = 0; col < cols; col++) {
            int left = (x0 > col * w / cols) ? x0 : col * w / cols;
            int right = (x1 < (col + 1) * w / cols) ? x1 : (col + 1) * w / cols;
            if (right <= left) {
                continue;
            }
            float a[9], b[9], c[9], d[9];
            live_vertex(a, left, top, w, h, bright);
            live_vertex(b, right, top, w, h, bright);
            live_vertex(c, right, bottom, w, h, bright);
            live_vertex(d, left, bottom, w, h, bright);
            rdpq_triangle(format, a, b, c);
            rdpq_triangle(format, a, c, d);
        }
    }
}

/* ---------- public API ---------- */

void theme_init (void) {
    if (initialized) {
        return;
    }
    initialized = true;

    theme_set_defaults(&theme);

    /* One of the built-in themes, or the player's own file from the card.
       With no file, and in safe mode, it is the stock look (Midnight). */
    int choice = safe_mode_active() ? BUILTIN_THEME_MIDNIGHT : options_get(OPTION_THEME);
    builtin_theme_apply(&theme, choice);
    from_sd = false;
    if (choice == BUILTIN_THEME_FROM_CARD) {
        from_sd = theme_parse_file(&theme, THEME_INI_PATH) || theme_parse_file(&theme, THEME_TXT_PATH);
    } else if (choice >= THEME_CHOICE_CARD) {
        const char *path = card_themes_path();     /* a theme folder in sd:/menu/themes; gone: Midnight */
        from_sd = path && theme_parse_file(&theme, path);
        debugf("theme: card theme %s\n", path ? path : "(folder gone, Midnight)");
    }
}

void theme_reload (void) {
    theme_background_suspend();     /* lets go of the old background... */
    theme_background_resume();      /* ...and the new one is built on the next draw */
    initialized = false;
    theme_init();
}

void theme_preview (const theme_t *t, bool background) {
    if (background) {
        theme_background_suspend();
        theme_background_resume();
        theme = *t;
    } else {
        theme_t keep = theme;   /* what the background was made from stays */
        theme = *t;
        theme.bg_type = keep.bg_type;
        theme.direction = keep.direction;
        theme.color1 = keep.color1;
        theme.color2 = keep.color2;
        theme.color3 = keep.color3;
        theme.use_color3 = keep.use_color3;
        theme.dither = keep.dither;
        memcpy(theme.image, keep.image, sizeof(theme.image));
        theme.pattern = keep.pattern;
        theme.pattern_color = keep.pattern_color;
        theme.pattern_size = keep.pattern_size;
        theme.pattern_opacity = keep.pattern_opacity;
    }
    initialized = true;
}

bool theme_drawn_live (const theme_t *t) {
    return live_possible(t);
}

/* Built on first draw, so the display is guaranteed to be set up by then. */
static void theme_ensure_background (void) {
    if (background_built) {
        return;
    }
    background_built = true;
    theme_init();
    int before = memory_log_used();
    if (live_possible(&theme)) {
        live_prepare(&theme);
    } else {
        theme_build_background(&theme);
    }
    memory_log_since(before, "theme background");
}

const theme_t *theme_get (void) {
    if (!initialized) {
        theme_init();
    }
    return &theme;
}

bool theme_loaded_from_sd (void) {
    return from_sd;
}

void theme_background_suspend (void) {
    suspended = true;
    if (background.buffer) {
        rspq_wait(); /* nothing may still be drawing from it */
        surface_free(&background);
        memset(&background, 0, sizeof(background));
    }
    if (live) {
        rspq_wait();
        live_forget();
    }
    background_built = false;
}

void theme_background_resume (void) {
    suspended = false;
}

/* Asked for by theme_background_dim() for the next draw only. */
static int dim_dark = 0;
static bool dim_applied = false;

void theme_background_dim (int dark) {
    dim_dark = (dark < 0) ? 0 : ((dark > 255) ? 255 : dark);
    dim_applied = false;
}

bool theme_background_dimmed (void) {
    return dim_applied;
}

void theme_background_draw_part (int x0, int y0, int x1, int y1) {
    if (live) {
        int w = display_get_width();
        int h = display_get_height();
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > w) x1 = w;
        if (y1 > h) y1 = h;
        if (x1 > x0 && y1 > y0) {
            rdpq_mode_push();
            live_draw(x0, y0, x1, y1, 255);
            rdpq_mode_pop();
        }
        return;
    }
    /* Whole background pixels only (it is stored at 1/BG_SCALE size). */
    x0 = (x0 < 0) ? 0 : (x0 / BG_SCALE) * BG_SCALE;
    y0 = (y0 < 0) ? 0 : (y0 / BG_SCALE) * BG_SCALE;
    x1 = ((x1 + BG_SCALE - 1) / BG_SCALE) * BG_SCALE;
    y1 = ((y1 + BG_SCALE - 1) / BG_SCALE) * BG_SCALE;
    if (x1 > (int) display_get_width()) x1 = display_get_width();
    if (y1 > (int) display_get_height()) y1 = display_get_height();
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    rdpq_mode_push();
    if (background.buffer) {
        rdpq_set_mode_standard();
        if (over_alpha < 255) {
            rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, over_alpha));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        } else {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        }
        rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
        rdpq_tex_blit(&background, x0, y0, &(rdpq_blitparms_t) {
            .s0 = x0 / BG_SCALE,
            .t0 = y0 / BG_SCALE,
            .width = (x1 - x0) / BG_SCALE,
            .height = (y1 - y0) / BG_SCALE,
            .scale_x = BG_SCALE,
            .scale_y = BG_SCALE,
        });
    } else if (over_alpha < 255) {
        rdpq_set_mode_standard();
        rdpq_set_prim_color(RGBA32(theme.color1.r, theme.color1.g, theme.color1.b, over_alpha));
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    } else {
        rdpq_set_mode_fill(theme.color1);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    }
    rdpq_mode_pop();
}

void theme_background_fade_part (int x0, int y0, int x1, int y1, int alpha) {
    if (alpha <= 0) {
        return;
    }
    over_alpha = (alpha > 255) ? 255 : alpha;
    theme_background_draw_part(x0, y0, x1, y1);
    over_alpha = 255;
}

void theme_background_draw (void) {
    if (!suspended) {
        theme_ensure_background();
    }
    rdpq_mode_push();
    if (live) {
        live_draw(0, 0, display_get_width(), display_get_height(), 255 - dim_dark);
    } else if (background.buffer) {
        /* Stretching needs the standard mode; the fast copy mode can't scale. */
        rdpq_set_mode_standard();
        if (dim_dark > 0) {
            /* Darkened as it is drawn, which costs the chip nothing extra. */
            int bright = 255 - dim_dark;
            rdpq_set_prim_color(RGBA32(bright, bright, bright, 0xFF));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        } else {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        }
        rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
        rdpq_tex_blit(&background, 0, 0, &(rdpq_blitparms_t) {
            .scale_x = BG_SCALE,
            .scale_y = BG_SCALE,
        });
    } else {
        int bright = 255 - dim_dark;
        rdpq_set_mode_fill(RGBA32(theme.color1.r * bright / 255, theme.color1.g * bright / 255, theme.color1.b * bright / 255, 0xFF));
        rdpq_fill_rectangle(0, 0, display_get_width(), display_get_height());
    }
    rdpq_mode_pop();
    dim_applied = (dim_dark > 0);
    dim_dark = 0;
}
