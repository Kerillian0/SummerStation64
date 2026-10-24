/**
 * @file hidden_games.h
 * @brief Games the player has hidden from the cover tabs.
 *
 * A hidden game is left out of Games, Homebrew, Recent and Favorites (and so
 * of Random game, the filters, the version list and the idle reader). The
 * file stays on the card and the Folders tab still shows it, with Z to unhide
 * it. Settings > Library counts them and can show them all again.
 *
 * Kept in sd:/menu/hidden.txt: "hidden 1", then one file per line, from the
 * top of the card ("N64(US)/Game.z64"). A renamed or moved file shows again.
 */

#ifndef HIDDEN_GAMES_H__
#define HIDDEN_GAMES_H__

#include <stdbool.h>

/** Most games that can be hidden. */
#define HIDDEN_GAMES_MAX    (256)

/** True if this game is hidden. `name` is from the top of the card ("N64(US)/Game.z64"). */
bool hidden_games_name (const char *name);

/** The same for a full path ("sd:/N64(US)/Game.z64"). */
bool hidden_games_is (const char *path);

/**
 * Hide a game (full path) or show it again, and save.
 * @return false if it could not be hidden (the list is full).
 */
bool hidden_games_set (const char *path, bool hidden);

/** How many games are hidden. */
int hidden_games_count (void);

/** Show every hidden game again, and save. */
void hidden_games_clear (void);

#endif /* HIDDEN_GAMES_H__ */
