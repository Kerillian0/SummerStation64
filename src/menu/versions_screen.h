/**
 * @file versions_screen.h
 * @brief A cover standing for several files (Game Versions: Grouped, Region
 * Copies: One Cover): A opens this screen over the Games screen instead of a
 * pop-up list.
 *
 * The cover grows into a column of covers, one per version, the Games
 * screen blurred and darkened behind (a small copy of it taken as the
 * screen opens, stretched smoothly). The selected version is large, the
 * others smaller above and below it, sliding as up and down move. Each
 * name sits to the right of its cover; the selected one also says how
 * often and when it was last played. Option `versions_layout` (Look >
 * Covers > Versions Column): the column on the left (default) or in the
 * middle. A picks (Game info, or starting it if the A press asked to), B
 * folds the column back into the cover.
 *
 * Covers come from baked art, else the SD card, one a frame; versions that
 * share a game code share one picture. The order is the one the browser
 * gives (the cover's own copy, the other copies, then hacks).
 */

#ifndef VERSIONS_SCREEN_H__
#define VERSIONS_SCREEN_H__

#include <stdbool.h>

#include "menu_state.h"

typedef enum {
    VERSIONS_LAYOUT_LEFT,
    VERSIONS_LAYOUT_CENTRE,
    VERSIONS_LAYOUT_COUNT,
} versions_layout_t;

/** The name of a `versions_layout` value, for Settings. */
const char *versions_screen_layout_name (int choice);

/**
 * Open it.
 * @param entries   the versions, in the order shown
 * @param names     their names as shown
 * @param n         how many
 * @param selected  the one selected first (the one played last)
 * @param launch    the A press asked to start the game straight away
 */
void versions_screen_open (menu_t *menu, entry_t **entries, const char **names, int n, int selected, bool launch);

/** It is showing (opening, open or folding away). */
bool versions_screen_active (void);

/** Read the buttons. @return true while it is showing (the browser does nothing else). */
bool versions_screen_process (menu_t *menu);

/** Before the frame is drawn into `d`: on the first frame, a small blurred copy of the Games screen still in it. */
void versions_screen_capture (surface_t *d);

/** Draw it (the whole screen). */
void versions_screen_draw (menu_t *menu);

/** After the frame: load the next cover. */
void versions_screen_after_draw (menu_t *menu);

/** Close at once and free its covers (the list is being read again, or the screen left). */
void versions_screen_close (void);

#endif /* VERSIONS_SCREEN_H__ */
