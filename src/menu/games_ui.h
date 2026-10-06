/**
 * @file games_ui.h
 * @brief Shared pieces of the redesigned Games screens: the tab bar with
 *        the clock, the title panel and the position bar.
 */

#ifndef GAMES_UI_H__
#define GAMES_UI_H__

#include "game_facts.h"
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

/** "You are here" bar: first letter, a track with a marker, and "13 of 79". */
void games_ui_position_draw (const char *title, int selected, int count);

#endif /* GAMES_UI_H__ */
