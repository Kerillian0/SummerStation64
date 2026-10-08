/**
 * @file start_menu.h
 * @brief The menu START opens (settings, Controller Pak manager, clock,
 *        information screens), for the tabs that are not the file browser.
 *
 * The browser screen has had this menu since the stock menu; this gives
 * the Recent and Favorites tabs the same one. The list is repeated here
 * rather than shared, to leave the stock browser code alone: keep the two
 * the same.
 */

#ifndef START_MENU_H__
#define START_MENU_H__

#include <stdbool.h>
#include "menu_state.h"

/** Call when the screen opens. */
void start_menu_init (void);

/**
 * Call first thing each frame.
 * @return True while the menu is open: the screen must not handle buttons itself.
 */
bool start_menu_process (menu_t *menu);

/** Open the menu (when START is pressed). */
void start_menu_show (void);

/** Draw it, last, over everything else. */
void start_menu_draw (void);

#endif /* START_MENU_H__ */
