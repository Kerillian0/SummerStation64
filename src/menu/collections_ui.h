/**
 * @file collections_ui.h
 * @brief The Collections tab's row of cards, one per collection.
 *
 * The selected card shows its first three games' covers fanned out like a
 * hand of cards, with the collection's name and game count on the title
 * panel; the cards either side show their names. Left / right move along
 * the row, A opens a collection (the Games list showing only its games,
 * games_filter_collection()), L / R switch tabs. Inside a collection a
 * quick tap of B comes back here; holding B still turns the 3D Box.
 *
 * The three covers are the only memory it uses (about 105 KB), loaded once
 * the selection rests and only while there is room; freed when the tab or
 * the card changes.
 */

#ifndef COLLECTIONS_UI_H__
#define COLLECTIONS_UI_H__

#include <stdbool.h>
#include "menu_state.h"

typedef enum {
    COLLECTIONS_UI_NONE,
    COLLECTIONS_UI_OPEN,        /**< A on a card: open the collection collections_ui_selected() */
    COLLECTIONS_UI_TAB_LEFT,    /**< L: the tab before */
    COLLECTIONS_UI_TAB_RIGHT,   /**< R: the tab after */
    COLLECTIONS_UI_SETTINGS,    /**< START: the START menu */
} collections_ui_action_t;

/** Read this frame's buttons on the row of cards. */
collections_ui_action_t collections_ui_process (menu_t *menu);

/** The selected card's collection, by its number (collections_id()). */
int collections_ui_selected (void);

/** Draw the row, title panel, position bar and hints (the tab bar is the caller's). */
void collections_ui_draw (menu_t *menu);

/** After the frame is handed to the graphics chip: load the selected card's covers, one a frame. */
void collections_ui_after_draw (menu_t *menu);

/** Free the covers (leaving the row of cards). */
void collections_ui_free (void);

/** Inside a collection: true once when B was tapped (let go quickly), to come back to the cards. */
bool collections_ui_back_tap (void);

#endif /* COLLECTIONS_UI_H__ */
