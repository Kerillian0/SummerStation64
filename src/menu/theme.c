#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "theme.h"
#include "safe_mode.h"

static theme_t theme;
static surface_t background;
static bool initialized = false;     /* settings loaded */
static bool background_built = false;
static bool from_sd = false;
static bool suspended = false;        /* memory lent to another screen */

static const uint8_t bayer4[16] = {
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5,
};

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

static uint8_t quantize5 (float v, float bias) {
    v += bias;
    if (v < 0.0f) v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (uint8_t) lroundf(v / 255.0f * 31.0f);
}

static void theme_build_background (const theme_t *t) {
    int w = display_get_width();
    int h = display_get_height();

    background = surface_alloc(FMT_RGBA16, w, h);

    /* Build each row in cached RAM, then copy it out in one go. */
    uint16_t *row = malloc(w * sizeof(uint16_t));
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

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            rgbf_t c;
            if (type == THEME_BG_GRADIENT) {
                float p = gradient_pos(t, x, y, w, h);
                if (t->use_color3) {
                    c = (p < 0.5f) ? lerp(c1, c3, p * 2.0f) : lerp(c3, c2, (p - 0.5f) * 2.0f);
                } else {
                    c = lerp(c1, c2, p);
                }
            } else {
                c = c1;
            }

            if (pa > 0.0f && pattern_hit(t, x, y)) {
                c = lerp(c, pc, pa);
            }

            float bias = t->dither ? ((bayer4[(y & 3) * 4 + (x & 3)] / 16.0f) - 0.5f) * 8.0f : 0.0f;
            uint8_t r = quantize5(c.r, bias);
            uint8_t g = quantize5(c.g, bias);
            uint8_t b = quantize5(c.b, bias);
            row[x] = (r << 11) | (g << 6) | (b << 1) | 1;
        }
        memcpy((uint8_t *) background.buffer + y * background.stride, row, w * sizeof(uint16_t));
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
    from_sd = !safe_mode_active() && (theme_load_ini(&theme, THEME_INI_PATH) || theme_load_ini(&theme, THEME_TXT_PATH));
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
        rdpq_set_mode_copy(false);
        rdpq_tex_blit(&background, 0, 0, NULL);
    } else {
        rdpq_set_mode_fill(theme.color1);
        rdpq_fill_rectangle(0, 0, display_get_width(), display_get_height());
    }
    rdpq_mode_pop();
}
