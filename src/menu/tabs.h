/**
 * @file tabs.h
 * @brief Which tabs are on the tab bar, and in what order.
 *
 * The bar has up to four places. The player picks what goes in each
 * (Settings > Tabs): Games, Favorites, Folders, Recent, or nothing. Out of
 * the box it is Games, Favorites, Folders. L and R step through them.
 *
 * Games and Folders are the same screen (the file browser) showing two
 * different lists, so this also keeps track of which of the two is wanted.
 */

#ifndef TABS_H__
#define TABS_H__

#include <stdbool.h>
#include "games_ui.h"
#include "menu_state.h"

/** What a place on the bar can hold (options `tab1` to `tab4` in options.ini). */
typedef enum {
    TAB_CHOICE_NONE,
    TAB_CHOICE_GAMES,
    TAB_CHOICE_FAVORITES,
    TAB_CHOICE_FOLDERS,
    TAB_CHOICE_RECENT,
    TAB_CHOICE_COUNT
} tab_choice_t;

/** Name of a choice, for the settings screen. */
const char *tabs_choice_name (int choice);

/**
 * The tabs on the bar, in order. A tab chosen twice counts once; if nothing
 * is chosen, the built-in order is used.
 * @param out Room for GAMES_TAB_COUNT tabs.
 * @return How many there are.
 */
int tabs_list (games_tab_t *out);

/** The tab the menu opens on. */
games_tab_t tabs_first (void);

/** The tab before (-1) or after (+1) `from`, going round at the ends. */
games_tab_t tabs_step (games_tab_t from, int direction);

/** Go to a tab: sets the next screen and, for Games/Folders, which list it shows. */
void tabs_open (menu_t *menu, games_tab_t tab);

/**
 * Covers slide in from the side when a tab is switched to. Call when L (-1)
 * or R (+1) changes tab, after tabs_open(). Going to Folders plays nothing.
 */
void tabs_slide_begin (menu_t *menu, int direction);

/**
 * How far the new tab's covers still have to travel: +1 (a full step to the
 * right of where they rest) or -1 at the start, 0 once they are in place.
 * The clock starts at the first call after tabs_slide_begin(), so a slow
 * first frame on the new tab does not eat the slide.
 */
float tabs_slide (void);

/** Whether the browser screen should show Folders (true) or Games (false). */
bool tabs_browser_shows_folders (void);
void tabs_browser_show_folders (bool folders);

#endif /* TABS_H__ */
