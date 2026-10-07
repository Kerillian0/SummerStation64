/**
 * @file intro.h
 * @brief The short intro shown when the console is switched on.
 *
 * The menu's name fades in over the theme background with a short tune,
 * then the first screen appears. Any button skips it. It is not shown after
 * pressing RESET (coming back from a game), in safe mode, or when a game is
 * set to start by itself. Feature `boot_animation`.
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

#endif /* INTRO_H__ */
