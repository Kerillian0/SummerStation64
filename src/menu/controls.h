/**
 * @file controls.h
 * @brief Button layout for the tabbed screens (Files, History, Favorites).
 *
 *   L / R        previous / next tab
 *   Z            options (was R)
 *   Left/Right   scroll the carousel (do nothing in a vertical list)
 *   Up/Down      scroll a vertical list; in the carousel they turn the box
 *                over, or scroll when the "updown_scroll" feature is on
 *   A            "quick_launch" on: tap starts the game, hold shows its info
 *                "hold_launch" on:  tap shows its info, hold starts the game
 *                both off:          shows the game info (stock behaviour)
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

/**
 * True once when up or down was pressed on the carousel while those don't
 * scroll ("updown_scroll" off): the box should turn over.
 */
bool controls_consume_flip_request (void);

/** Hint bar text for A on a game, matching the launch mode in use. */
const char *controls_rom_hint (void);

/**
 * What a tap and a hold of A do on a game, as single words for the button
 * hints. `hold` is set to NULL when holding does nothing special.
 */
void controls_rom_actions (const char **tap, const char **hold);

/**
 * True once if the A press that opened the game info screen asked to start
 * the game straight away. Asked by the game info screen when it opens.
 */
bool controls_consume_launch_request (void);

#endif /* CONTROLS_H__ */
