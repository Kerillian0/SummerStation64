/**
 * @file controls.h
 * @brief Button layout for the tabbed screens (Files, History, Favorites).
 *
 *   L / R        previous / next tab
 *   Z            options (was R)
 *   Left/Right   scroll the carousel (do nothing in a vertical list)
 *   Up/Down      scroll a vertical list; in the carousel only when the
 *                "updown_scroll" feature is on
 */

#ifndef CONTROLS_H__
#define CONTROLS_H__

#include <stdbool.h>

#include "menu_state.h"

/**
 * Rewrite this frame's actions so the stock screen code can stay as it is:
 * afterwards go_up/go_down mean "scroll" and go_left/go_right mean
 * "previous/next tab". Call it after any open context menu was handled.
 *
 * @param horizontal True if the screen scrolls sideways (carousel).
 */
void controls_remap_tabs (menu_t *menu, bool horizontal);

#endif /* CONTROLS_H__ */
