/**
 * @file games_ui.h
 * @brief Shared pieces of the redesigned Games screens: the tab bar with
 *        the clock, the title panel and the position bar.
 */

#ifndef GAMES_UI_H__
#define GAMES_UI_H__

#include "fonts.h"
#include "game_facts.h"
#include "ui_components/constants.h"
#include "menu_state.h"

typedef enum {
    GAMES_TAB_GAMES,
    GAMES_TAB_RECENT,
    GAMES_TAB_FAVORITES,
    GAMES_TAB_COUNT
} games_tab_t;

/** Bottom edge of the tab bar, for laying out what goes under it. */
#define GAMES_UI_TOPBAR_BOTTOM  (56)

/** Where a list under the tab bar starts (Recent, Favorites). */
#define GAMES_UI_LIST_TOP       (GAMES_UI_TOPBAR_BOTTOM + 6)

/** Tab bar across the top: L, the three tabs, R, and the clock on the right. */
void games_ui_topbar_draw (menu_t *menu, games_tab_t selected);

/**
 * Dark panel with the selected entry's name. Under it: badges for a game
 * whose facts are known, otherwise the plain line of detail.
 */
void games_ui_title_panel_draw (const char *title, const char *detail, const game_facts_t *facts);

/**
 * A word in a small box, as wide as its text (`boxed` false draws the word
 * alone). @return Where the next one goes.
 */
int games_ui_badge (int x, int y, const char *label, menu_font_style_t style, bool boxed);

/** "You are here" bar: first letter, a track with a marker, and "13 of 79". */
void games_ui_position_draw (const char *title, int selected, int count);

/**
 * The title panel and the button hints share these edges: a little wider
 * than the position bar, and well clear of the edges a CRT hides.
 */
#define GAMES_UI_CONTENT_X0 (VISIBLE_AREA_X0 + 48)
#define GAMES_UI_CONTENT_X1 (VISIBLE_AREA_X1 - 48)

/** Left edge of the button hints along the bottom. They sit in two rows. */
#define GAMES_UI_HINTS_X    (GAMES_UI_CONTENT_X0)

/**
 * One button hint: the button in a small box, then what it does.
 * @param row 0 for the upper row, 1 for the lower one.
 * @return Where the next hint in that row goes.
 */
int games_ui_hint_draw (int x, int row, const char *button, const char *action);

/** How much room a hint takes. */
int games_ui_hint_width (const char *button, const char *action);

/** The same, placed against the right edge. */
void games_ui_hint_right_draw (int row, const char *button, const char *action);

#endif /* GAMES_UI_H__ */
