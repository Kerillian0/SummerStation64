/**
 * @file intro.h
 * @brief The short intro shown when the console is switched on.
 *
 * The menu's name fades in over the theme background with a short tune,
 * then the first screen appears. Any button skips it. It is not shown after
 * pressing RESET (coming back from a game), in safe mode, or when a game is
 * set to start by itself. Feature `boot_animation`.
 *
 * Also the fade-in that follows every start, intro or not: the first screen
 * comes up from black and the background music rises with it, over two
 * seconds or one (option `fade_speed`). Feature `fade_in`.
 */

#ifndef INTRO_H__
#define INTRO_H__

#include <stdbool.h>
#include <libdragon.h>
#include "menu_state.h"

/**
 * Call at the end of the startup screen's set-up, once it has chosen the
 * first screen. If the intro should play, the startup screen stays up and
 * that first screen is opened when the intro ends.
 */
void intro_begin (menu_t *menu);

/**
 * Draw one frame of the intro.
 * @return false if the intro is not playing (the caller draws as usual).
 */
bool intro_display (menu_t *menu, surface_t *display);

/** Call once a frame: brings the background music up during the fade-in. */
void intro_poll (void);

/**
 * Call at the end of a screen's drawing, just before the picture is shown:
 * darkens it while the fade-in (feature `fade_in`) is running.
 */
void intro_fade_draw (void);

/** Number of fade-in speeds, and the name of each, for the settings screen. */
#define INTRO_FADE_SPEED_COUNT  (2)
const char *intro_fade_speed_name (int choice);

#endif /* INTRO_H__ */
