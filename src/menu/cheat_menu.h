/**
 * @file cheat_menu.h
 * @brief Z on a game's Game info: its cheats, to switch on and off before
 *        starting it.
 *
 * The cheats are the ones in the game's cheat file ("Game.datel" beside it,
 * written by the Companion's Cheats tab or by hand). Each cheat is a line
 * with a name, plus any nameless lines under it; a switched-off cheat has
 * ": " in front of its lines, as the stock loader reads it. The file is
 * written once, when the list closes, and only if something changed.
 * Switching any cheat on turns Use Cheats on for the game; switching them
 * all off turns it off.
 */

#ifndef CHEAT_MENU_H__
#define CHEAT_MENU_H__

#include <stdbool.h>
#include "menu_state.h"

/** True if the game at this full path has a cheat file (Game info shows "Z Cheats"). */
bool cheat_menu_available (const char *rom_path);

/** Open the list for the game being shown (menu->load). */
void cheat_menu_open (menu_t *menu);

/** While the list shows it has the controls: true if it took this frame's. */
bool cheat_menu_process (menu_t *menu);

/** Draw the list while it shows. */
void cheat_menu_draw (void);

#endif /* CHEAT_MENU_H__ */
