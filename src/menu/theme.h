/**
 * @file theme.h
 * @brief Theme loading and background generation for the menu.
 *
 * Reads sd:/menu/theme/theme.ini (written by the SC64 Theme Maker) and
 * builds the background once at startup. Missing or invalid keys fall
 * back to the built-in default theme.
 */

#ifndef THEME_H__
#define THEME_H__

#include <stdbool.h>
#include <libdragon.h>

#include "menu_features.h"

#define THEME_INI_PATH  "sd:/menu/theme/theme.ini"
#define THEME_TXT_PATH  "sd:/menu/theme/theme.txt"  /* as saved by the Theme Maker */

typedef enum {
    THEME_BG_SOLID,
    THEME_BG_GRADIENT,
    THEME_BG_IMAGE,
} theme_bg_type_t;

typedef enum {
    THEME_DIR_VERTICAL,
    THEME_DIR_HORIZONTAL,
    THEME_DIR_DIAGONAL,
    THEME_DIR_RADIAL,
} theme_dir_t;

typedef enum {
    THEME_PATTERN_NONE,
    THEME_PATTERN_STRIPES,
    THEME_PATTERN_DIAGONAL,
    THEME_PATTERN_CHECKER,
    THEME_PATTERN_DOTS,
    THEME_PATTERN_GRID,
    THEME_PATTERN_SCANLINES,
} theme_pattern_t;

typedef struct {
    char name[41];
    char author[41];

    color_t text;
    color_t text_dim;
    color_t accent;
    color_t panel;

    theme_bg_type_t bg_type;
    theme_dir_t direction;
    color_t color1;
    color_t color2;
    color_t color3;
    bool use_color3;
    bool dither;
    char image[64];

    theme_pattern_t pattern;
    color_t pattern_color;
    int pattern_size;
    int pattern_opacity; /* 0-100 */

    int8_t features[FEATURE_COUNT]; /* FEATURE_UNSET, 0 or 1 */
} theme_t;

/** Load theme settings from the SD card. Safe to call more than once. */
void theme_init (void);

/** Current theme (defaults if nothing was loaded). */
const theme_t *theme_get (void);

/** True if theme.ini was found and read from the SD card. */
bool theme_loaded_from_sd (void);

/** Draw the pre-built background. Call right after rdpq_attach(). */
void theme_background_draw (void);

#endif /* THEME_H__ */
