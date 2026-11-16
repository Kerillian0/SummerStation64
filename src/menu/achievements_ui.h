/**
 * @file achievements_ui.h
 * @brief Game info's Achievements page: L flips between the game's info and
 * a grid of its achievement badges (greyed while locked), the selected one's
 * name, requirement and points in a panel underneath, and how far the player
 * has got. Only for games `ra_bridge.exe --prepare` found a set for
 * (achievements.h); a copy RetroAchievements doesn't support says so.
 */

#ifndef ACHIEVEMENTS_UI_H__
#define ACHIEVEMENTS_UI_H__

#include <stdbool.h>

#include "menu_state.h"

/** Game info opened for the ROM at this full path. */
void achievements_ui_open (const char *rom_path);

/** Game info closed: everything freed. */
void achievements_ui_close (void);

/** The game has a page (L shows the hint). */
bool achievements_ui_available (void);

/** The page is showing instead of the info. */
bool achievements_ui_showing (void);

/** Buttons: L flips pages; on the page the D-Pad moves. @return true if the press was taken. */
bool achievements_ui_process (menu_t *menu);

/** Draw the page (the shared background is already drawn). */
void achievements_ui_draw (menu_t *menu);

#endif
