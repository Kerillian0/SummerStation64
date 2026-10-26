/**
 * @file favorite_toggle.h
 * @brief Favorite or unfavorite a game straight from its cover (Z menu),
 *        without opening its Game info.
 *
 * Cartridge games and 64DD disks only: the stock Favorites list has no kind
 * for emulator games. A disk is kept without its cartridge; its pairing
 * (dd_pair.h) picks the cartridge when it is opened.
 */

#ifndef FAVORITE_TOGGLE_H__
#define FAVORITE_TOGGLE_H__

#include <stdbool.h>
#include "menu_state.h"

/** True if this kind of entry can be a favorite. */
bool favorite_toggle_allowed (const entry_t *e);

/** True if the game at this full path is in Favorites. */
bool favorite_toggle_is (menu_t *menu, const char *path);

/** "Add to Favorites" or "Remove from Favorites" for the game at this path. */
const char *favorite_toggle_text (menu_t *menu, const char *path);

/**
 * Adds the game if it isn't a favorite, removes it if it is, and saves.
 * @return True if it is a favorite now.
 */
bool favorite_toggle (menu_t *menu, const char *path, bool disk);

#endif /* FAVORITE_TOGGLE_H__ */
