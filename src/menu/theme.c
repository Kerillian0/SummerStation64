#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "theme.h"
#include "safe_mode.h"
#include "builtin_themes.h"
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
    t->pattern = THEME_PATTERN_NONE;
    t->pattern_color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
    t->pattern_size = 16;
    t->pattern_opacity = 10;
    for (int i = 0; i < FEATURE_COUNT; i++) {
        t->features[i] = FEATURE_UNSET;
    }
}

/* ---------- INI parsing ---------- */

static char *trim (char *s) {
    while (isspace((unsigned char) *s)) {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char) end[-1])) {
        end--;
    }
    *end = '\0';
    return s;
}

static bool parse_hex_color (const char *v, color_t *out) {
    if (*v == '#') {
        v++;
    }
    if (strlen(v) != 6) {
        return false;
    }
    char *end;
    unsigned long n = strtoul(v, &end, 16);
    if (*end != '\0') {
        return false;
    }
    *out = RGBA32((n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF, 0xFF);
    return true;
}

static void parse_int (const char *v, int *out, int min, int max) {
    char *end;
    long n = strtol(v, &end, 10);
    if (end != v) {
        if (n < min) n = min;
        if (n > max) n = max;
        *out = (int) n;
    }
}

static void copy_str (char *dst, size_t size, const char *src) {
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static void apply_key (theme_t *t, const char *section, const char *key, const char *v) {
    if (!strcasecmp(section, "theme")) {
        if (!strcasecmp(key, "name")) copy_str(t->name, sizeof(t->name), v);
        else if (!strcasecmp(key, "author")) copy_str(t->author, sizeof(t->author), v);
    } else if (!strcasecmp(section, "colors")) {
        if (!strcasecmp(key, "text")) parse_hex_color(v, &t->text);
        else if (!strcasecmp(key, "text_dim")) parse_hex_color(v, &t->text_dim);
        else if (!strcasecmp(key, "accent")) parse_hex_color(v, &t->accent);
        else if (!strcasecmp(key, "panel")) parse_hex_color(v, &t->panel);
        else if (!strcasecmp(key, "border")) parse_hex_color(v, &t->border);
        else if (!strcasecmp(key, "highlight")) parse_hex_color(v, &t->highlight);
        else if (!strcasecmp(key, "tab_active")) parse_hex_color(v, &t->tab_active);
        else if (!strcasecmp(key, "tab_inactive")) parse_hex_color(v, &t->tab_inactive);
        else if (!strcasecmp(key, "tab_active_border")) parse_hex_color(v, &t->tab_active_border);
        else if (!strcasecmp(key, "tab_inactive_border")) parse_hex_color(v, &t->tab_inactive_border);
    } else if (!strcasecmp(section, "background")) {
        if (!strcasecmp(key, "type")) {
            if (!strcasecmp(v, "solid")) t->bg_type = THEME_BG_SOLID;
            else if (!strcasecmp(v, "gradient")) t->bg_type = THEME_BG_GRADIENT;
            else if (!strcasecmp(v, "image")) t->bg_type = THEME_BG_IMAGE;
            else if (!strcasecmp(v, "ocean")) t->bg_type = THEME_BG_OCEAN;
        } else if (!strcasecmp(key, "direction")) {
            if (!strcasecmp(v, "vertical")) t->direction = THEME_DIR_VERTICAL;
            else if (!strcasecmp(v, "horizontal")) t->direction = THEME_DIR_HORIZONTAL;
            else if (!strcasecmp(v, "diagonal")) t->direction = THEME_DIR_DIAGONAL;
            else if (!strcasecmp(v, "radial")) t->direction = THEME_DIR_RADIAL;
        } else if (!strcasecmp(key, "color1")) {
            parse_hex_color(v, &t->color1);
        } else if (!strcasecmp(key, "color2")) {
            parse_hex_color(v, &t->color2);
        } else if (!strcasecmp(key, "color3")) {
            t->use_color3 = parse_hex_color(v, &t->color3);
        } else if (!strcasecmp(key, "dither")) {
            t->dither = (atoi(v) != 0);
        } else if (!strcasecmp(key, "image")) {
            copy_str(t->image, sizeof(t->image), v);
        }
    } else if (!strcasecmp(section, "features")) {
        feature_t f = feature_from_key(key);
        if (f < FEATURE_COUNT) {
            t->features[f] = (atoi(v) != 0) ? 1 : 0;
        }
    } else if (!strcasecmp(section, "pattern")) {
        if (!strcasecmp(key, "style")) {
            if (!strcasecmp(v, "none")) t->pattern = THEME_PATTERN_NONE;
            else if (!strcasecmp(v, "stripes")) t->pattern = THEME_PATTERN_STRIPES;
            else if (!strcasecmp(v, "diagonal")) t->pattern = THEME_PATTERN_DIAGONAL;
            else if (!strcasecmp(v, "checker")) t->pattern = THEME_PATTERN_CHECKER;
            else if (!strcasecmp(v, "dots")) t->pattern = THEME_PATTERN_DOTS;
            else if (!strcasecmp(v, "grid")) t->pattern = THEME_PATTERN_GRID;
            else if (!strcasecmp(v, "scanlines")) t->pattern = THEME_PATTERN_SCANLINES;
        } else if (!strcasecmp(key, "color")) {
            parse_hex_color(v, &t->pattern_color);
        } else if (!strcasecmp(key, "size")) {
            parse_int(v, &t->pattern_size, 2, 128);
        } else if (!strcasecmp(key, "opacity")) {
            parse_int(v, &t->pattern_opacity, 0, 100);
        }
    }
}

static bool theme_load_ini (theme_t *t, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }

    char line[160];
    char section[32] = "";

    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (*s == '\0' || *s == ';' || *s == '#') {
            continue;
        }
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                copy_str(section, sizeof(section), trim(s + 1));
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        apply_key(t, section, trim(s), trim(eq + 1));
    }

    fclose(f);
    return true;
}

/* ---------- background generation ---------- */

typedef struct { float r, g, b; } rgbf_t;

static rgbf_t to_f (color_t c) {
    return (rgbf_t) { c.r, c.g, c.b };
}

static rgbf_t lerp (rgbf_t a, rgbf_t b, float t) {
    return (rgbf_t) { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

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

/* Water seen from just above it, running away to a horizon near the top of
   the screen, with a net of pale foam lines across it. The lines are the
   borders between scattered points' territories (which gives the rounded,
   uneven cells of light on water), bent a little so they aren't straight.
   Tried out on a PC first with the same sums. */
#define OCEAN_HORIZON   (0.10f)     /* how far down the screen the horizon is */
#define OCEAN_ACROSS    (2.2f)      /* how many cells fit across at the very bottom, roughly halved */
#define OCEAN_LINE      (0.06f)     /* foam is solid nearer a border than this... */
#define OCEAN_LINE_SOFT (0.11f)     /* ...and gone beyond this */
#define OCEAN_HAZE      (0.30f)     /* the far part of the water, this much of it, fades into the sky */

/* A fixed scatter of points: the same "random" spot for a cell every time. */
static uint32_t ocean_scatter (int ix, int iz) {
    uint32_t h = ((uint32_t) ix * 374761393u) + ((uint32_t) iz * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* How much nearer the nearest point is than the next nearest: 0 on a border. */
static float ocean_border (float x, float z) {
    int ix = (int) floorf(x);
    int iz = (int) floorf(z);
    float nearest = 9.0f, second = 9.0f;

    for (int dz = -1; dz <= 1; dz++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint32_t h = ocean_scatter(ix + dx, iz + dz);
            float px = (ix + dx) + ((h & 0xFFFF) / 65536.0f);
            float pz = (iz + dz) + (((h >> 16) & 0xFFFF) / 65536.0f);
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

static rgbf_t ocean_color (int x, int y, int w, int h, rgbf_t water, rgbf_t foam, rgbf_t sky) {
    float horizon = OCEAN_HORIZON * h;
    rgbf_t white = { 1.0f, 1.0f, 1.0f };
    rgbf_t haze = lerp(sky, white, 0.45f);

    if (y < horizon) {
        return lerp(sky, haze, y / horizon);
    }

    /* Each row of the screen is a line across the water; rows nearer the
       horizon are further away, so the same cells look smaller there. */
    float down = (y - horizon) + 1.0f;
    float away = (OCEAN_ACROSS * 1.1f * h) / down;
    float side = ((x - (w / 2)) * OCEAN_ACROSS) / down;

    float border = ocean_border(side + (0.22f * sinf((away * 2.3f) + (side * 1.1f))), away + (0.22f * sinf((side * 1.9f) - (away * 0.7f))));
    float line = (border > OCEAN_LINE_SOFT) ? 0.0f : ((border < OCEAN_LINE) ? 1.0f : ((OCEAN_LINE_SOFT - border) / (OCEAN_LINE_SOFT - OCEAN_LINE)));

    float near = down / (h - horizon);
    float fog = 1.0f - (near / OCEAN_HAZE);
    return lerp(lerp(water, foam, line), haze, (fog > 0.0f) ? fog : 0.0f);
}

static uint32_t to_byte (float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (uint32_t) lroundf(v);
}

static void theme_build_background (const theme_t *t) {
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
    rgbf_t c3 = to_f(t->color3);
    rgbf_t pc = to_f(t->pattern_color);
    float pa = t->pattern_opacity / 100.0f;

    /* Image backgrounds aren't decoded yet: fall back to color1. */
    theme_bg_type_t type = (t->bg_type == THEME_BG_IMAGE) ? THEME_BG_SOLID : t->bg_type;

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            /* Gradient and pattern are worked out in screen positions, so
               pattern sizes in theme.ini still mean screen pixels. */
            int x = bx * BG_SCALE;
            int y = by * BG_SCALE;
            rgbf_t c;
            if (type == THEME_BG_GRADIENT) {
                float p = gradient_pos(t, x, y, w, h);
                if (t->use_color3) {
                    c = (p < 0.5f) ? lerp(c1, c3, p * 2.0f) : lerp(c3, c2, (p - 0.5f) * 2.0f);
                } else {
                    c = lerp(c1, c2, p);
                }
            } else if (type == THEME_BG_OCEAN) {
                c = ocean_color(x, y, w, h, c1, c2, c3);
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
}

/* ---------- public API ---------- */

void theme_init (void) {
    if (initialized) {
        return;
    }
    initialized = true;

    theme_set_defaults(&theme);

    /* One of the built-in themes, or the player's own file from the card.
       With no file, and in safe mode, it is the stock look (Sunset). */
    int choice = safe_mode_active() ? BUILTIN_THEME_SUNSET : options_get(OPTION_THEME);
    builtin_theme_apply(&theme, choice);
    from_sd = false;
    if (choice == BUILTIN_THEME_FROM_CARD) {
        from_sd = theme_load_ini(&theme, THEME_INI_PATH) || theme_load_ini(&theme, THEME_TXT_PATH);
    }
}

void theme_reload (void) {
    theme_background_suspend();     /* lets go of the old background... */
    theme_background_resume();      /* ...and the new one is built on the next draw */
    initialized = false;
    theme_init();
}

/* Built on first draw, so the display is guaranteed to be set up by then. */
static void theme_ensure_background (void) {
    if (background_built) {
        return;
    }
    background_built = true;
    theme_init();
    theme_build_background(&theme);
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
    background_built = false;
}

void theme_background_resume (void) {
    suspended = false;
}

void theme_background_draw (void) {
    if (!suspended) {
        theme_ensure_background();
    }
    rdpq_mode_push();
    if (background.buffer) {
        /* Stretching needs the standard mode; the fast copy mode can't scale. */
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_mode_dithering(theme.dither ? DITHER_BAYER_NONE : DITHER_NONE_NONE);
        rdpq_tex_blit(&background, 0, 0, &(rdpq_blitparms_t) {
            .scale_x = BG_SCALE,
            .scale_y = BG_SCALE,
        });
    } else {
        rdpq_set_mode_fill(theme.color1);
        rdpq_fill_rectangle(0, 0, display_get_width(), display_get_height());
    }
    rdpq_mode_pop();
}
