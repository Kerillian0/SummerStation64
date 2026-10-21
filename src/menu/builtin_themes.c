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
    color_t border, tab_active_border, tab_inactive_border;    /* left out (all zero): the stock frame colors */
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
    [BUILTIN_THEME_MIDNIGHT] = {       /* the owner's own theme, seen in the project's pictures; the default since 2026-10-21 (took Night Drive's place) */
        .name = "Midnight",
        .text = RGB(0xFFFFFF), .text_dim = RGB(0x999999), .accent = RGB(0xD336F2), .panel = RGB(0x262626),
        .highlight = RGB(0x7F7F7F), .tab_active = RGB(0x6F6F6F), .tab_inactive = RGB(0x3F3F3F),
        .direction = THEME_DIR_VERTICAL,
        .top = RGB(0x431B4B), .bottom = RGB(0x0B0E14),
        .pattern = THEME_PATTERN_GRID, .pattern_color = RGB(0xBF36AD), .pattern_size = 64, .pattern_opacity = 43,
    },
    [BUILTIN_THEME_BEACH] = {
        .name = "Beach",
        .text = RGB(0xFFFFF4), .text_dim = RGB(0xD8E8E8), .accent = RGB(0xFF7A50), .panel = RGB(0x0C243C),
        .highlight = RGB(0x1C6C8C), .tab_active = RGB(0x1C6C8C), .tab_inactive = RGB(0x103C5C),
        .direction = THEME_DIR_VERTICAL,
        .top = RGB(0x1A5C9A), .middle = RGB(0x127C80), .bottom = RGB(0xB08C52), .has_middle = true,
        .pattern = THEME_PATTERN_NONE, .pattern_color = RGB(0xFFFFFF), .pattern_size = 16, .pattern_opacity = 10,
    },
    [BUILTIN_THEME_MINT_GRID] = {      /* from the Theme Maker's presets */
        .name = "Mint Grid",
        .text = RGB(0xEAFBF6), .text_dim = RGB(0xA6D4C8), .accent = RGB(0x7FE0C3), .panel = RGB(0x10302C),
        .highlight = RGB(0x1F5A50), .tab_active = RGB(0x1F6A5C), .tab_inactive = RGB(0x0F3B36),
        .border = RGB(0x7FE0C3), .tab_active_border = RGB(0x7FE0C3), .tab_inactive_border = RGB(0x2C6A5E),
        .direction = THEME_DIR_VERTICAL,
        .top = RGB(0x0F3B36), .bottom = RGB(0x071A18),
        .pattern = THEME_PATTERN_GRID, .pattern_color = RGB(0x7FE0C3), .pattern_size = 32, .pattern_opacity = 12,
    },
    [BUILTIN_THEME_GRAPE] = {          /* from the Theme Maker's presets */
        .name = "Grape",
        .text = RGB(0xF4EEFF), .text_dim = RGB(0xC7B9E4), .accent = RGB(0xFF8AD8), .panel = RGB(0x24163D),
        .highlight = RGB(0x5A3A8C), .tab_active = RGB(0x6A3FA8), .tab_inactive = RGB(0x2E1B4E),
        .border = RGB(0xC7B9E4), .tab_active_border = RGB(0xFF8AD8), .tab_inactive_border = RGB(0x5A4580),
        .direction = THEME_DIR_RADIAL,
        .top = RGB(0x4A2A7A), .bottom = RGB(0x150B26),
        .pattern = THEME_PATTERN_CHECKER, .pattern_color = RGB(0xFFFFFF), .pattern_size = 24, .pattern_opacity = 5,
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
        choice = BUILTIN_THEME_MIDNIGHT;     /* the default look */
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
    if (look->border.a) {
        theme->border = look->border;
        theme->tab_active_border = look->tab_active_border;
        theme->tab_inactive_border = look->tab_inactive_border;
    }
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
