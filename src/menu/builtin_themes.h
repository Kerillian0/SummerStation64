/**
 * @file builtin_themes.h
 * @brief The themes that come with the menu.
 *
 * A small set of summer looks to choose from in Settings > Display, with no
 * files needed on the SD card. The first choice, "From SD Card", uses the
 * player's own theme.ini as before; with no such file it shows Sunset,
 * which is the menu's stock look.
 */

#ifndef BUILTIN_THEMES_H__
#define BUILTIN_THEMES_H__

#include "theme.h"

/** Option `theme` in options.ini. */
typedef enum {
    BUILTIN_THEME_FROM_CARD,    /**< sd:/menu/theme/theme.ini, or Sunset if there is none (the default) */
    BUILTIN_THEME_SUNSET,
    BUILTIN_THEME_NIGHT_DRIVE,
    BUILTIN_THEME_BEACH,
    BUILTIN_THEME_OCEAN,
    BUILTIN_THEME_COUNT
} builtin_theme_t;

/** Name of a choice, for the settings screen. */
const char *builtin_theme_name (int choice);

/** Fill in a theme's colors, background and pattern. FROM_CARD gives Sunset. */
void builtin_theme_apply (theme_t *theme, int choice);

#endif /* BUILTIN_THEMES_H__ */
