/**
 * @file cover_row.h
 * @brief A row of covers for a saved list of games (the Recent and Favorites tabs).
 *
 * Shows the games of a history or favorites list the way the Games screen
 * shows a folder: covers with box art, the title panel, badges and the
 * position bar. Left and right move along the row; up and down turn the
 * box over.
 */

#ifndef COVER_ROW_H__
#define COVER_ROW_H__

#include "bookkeeping.h"
#include "menu_state.h"

/**
 * Start showing a list. Empty places in it are left out.
 * @param items The saved list.
 * @param count How many places it has.
 */
void cover_row_open (menu_t *menu, bookkeeping_item_t *items, int count);

/**
 * Handle left/right and up/down. Call after controls_remap_tabs(menu, true).
 * @return The selected game's place in the saved list, or -1 if it is empty.
 */
int cover_row_process (menu_t *menu);

/**
 * Draw the covers and the button hints.
 * @param empty_message Shown when the list has no games.
 * @param z_action What Z does here ("Remove"), or NULL if nothing.
 */
void cover_row_draw (menu_t *menu, const char *empty_message, const char *z_action);

/** Stop showing the list and free its box art. */
void cover_row_close (void);

#endif /* COVER_ROW_H__ */
