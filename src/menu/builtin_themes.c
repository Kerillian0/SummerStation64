/**
 * @file builtin_themes.c
 * @brief The themes that come with the menu.
 */

#include <string.h>

#include "builtin_themes.h"

#define RGB(hex)    { ((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF, 0xFF }

typedef struct {
    const char *name;
    color_t text, text_dim, accent, panel;
    color_t highlight, tab_active, tab_inactive;
    theme_dir_t direction;
    color_t top, bottom, middle;    /* middle is used when has_middle is set */
    bool has_middle;
    bool ocean;                     /* the ocean background: top = water, bottom = foam */
    theme_pattern_t pattern;
    color_t pattern_color;
    int pattern_size;
    int pattern_opacity;
} look_t;

/* Backgrounds are kept on the dark side on purpose: some text sits straight
   on the background, and a composite TV smears light text on light colors. */
static const look_t looks[BUILTIN_THEME_COUNT] = {
    [BUILTIN_THEME_SUNSET] = {
        .name = "Sunset",
        .text = RGB(0xFFF4E6), .text_dim = RGB(0xE0B8C8), .accent = RGB(0xFFD23C), .panel = RGB(0x240C30),
        .highlight = RGB(0x9A3C6E), .tab_active = RGB(0x9A3C6E), .tab_inactive = RGB(0x4A1C50),
        .direction = THEME_DIR_VERTICAL,
        .top = RGB(0x1E0C4A), .middle = RGB(0x8A246E), .bottom = RGB(0xD8622C), .has_middle = true,
        .pattern = THEME_PATTERN_SCANLINES, .pattern_color = RGB(0xFFFFFF), .pattern_size = 8, .pattern_opacity = 6,
    },
    [BUILTIN_THEME_NIGHT_DRIVE] = {
        .name = "Night Drive",
        .text = RGB(0xF2E8FF), .text_dim = RGB(0xB8A0D0), .accent = RGB(0xFF6AD5), .panel = RGB(0x160824),
        .highlight = RGB(0x6A2A8A), .tab_active = RGB(0x6A2A8A), .tab_inactive = RGB(0x301448),
        .direction = THEME_DIR_VERTICAL,
        .top = RGB(0x2A0A4A), .bottom = RGB(0x0A0418),
        .pattern = THEME_PATTERN_GRID, .pattern_color = RGB(0xE050E0), .pattern_size = 32, .pattern_opacity = 45,
    },
    [BUILTIN_THEME_BEACH] = {
        .name = "Beach",
        .text = RGB(0xFFFFF4), .text_dim = RGB(0xD8E8E8), .accent = RGB(0xFF7A50), .panel = RGB(0x0C243C),
        .highlight = RGB(0x1C6C8C), .tab_active = RGB(0x1C6C8C), .tab_inactive = RGB(0x103C5C),
        .direction = THEME_DIR_VERTICAL,
        .top = RGB(0x1A5C9A), .middle = RGB(0x127C80), .bottom = RGB(0xB08C52), .has_middle = true,
        .pattern = THEME_PATTERN_NONE, .pattern_color = RGB(0xFFFFFF), .pattern_size = 16, .pattern_opacity = 10,
    },
    [BUILTIN_THEME_OCEAN] = {
        .name = "Ocean",
        .text = RGB(0xFFFFFF), .text_dim = RGB(0xD4F0F8), .accent = RGB(0xFFD84A), .panel = RGB(0x062448),
        .highlight = RGB(0x1870A8), .tab_active = RGB(0x1870A8), .tab_inactive = RGB(0x0A3C68),
        .ocean = true,      /* water and foam in place of top and bottom */
        .top = RGB(0x0C78BE), .bottom = RGB(0xA8E4EE), .middle = RGB(0x8CE6EE),
        .pattern = THEME_PATTERN_NONE, .pattern_color = RGB(0xFFFFFF), .pattern_size = 16, .pattern_opacity = 0,
    },
};

const char *builtin_theme_name (int choice) {
    if (choice <= BUILTIN_THEME_FROM_CARD || choice >= BUILTIN_THEME_COUNT) {
        return "From SD Card";
    }
    return looks[choice].name;
}

void builtin_theme_apply (theme_t *theme, int choice) {
    if (choice <= BUILTIN_THEME_FROM_CARD || choice >= BUILTIN_THEME_COUNT) {
        choice = BUILTIN_THEME_SUNSET;
    }
    const look_t *look = &looks[choice];

    strncpy(theme->name, look->name, sizeof(theme->name) - 1);
    theme->text = look->text;
    theme->text_dim = look->text_dim;
    theme->accent = look->accent;
    theme->panel = look->panel;
    theme->highlight = look->highlight;
    theme->tab_active = look->tab_active;
    theme->tab_inactive = look->tab_inactive;
    theme->bg_type = look->ocean ? THEME_BG_OCEAN : THEME_BG_GRADIENT;
    theme->direction = look->direction;
    theme->color1 = look->top;
    theme->color2 = look->bottom;
    theme->color3 = look->middle;
    theme->use_color3 = look->has_middle;
    theme->dither = true;
    theme->pattern = look->pattern;
    theme->pattern_color = look->pattern_color;
    theme->pattern_size = look->pattern_size;
    theme->pattern_opacity = look->pattern_opacity;
}
