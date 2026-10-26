/**
 * @file collections.h
 * @brief Groups of games for the Collections tab, worked out by the menu.
 *
 * Series (The Legend of Zelda, Mario Party, Banjo, ...) come from a table
 * built into the menu file (rom:/series.txt, made by
 * assets/data/make_series.py), matched by exact game code, so a hack or a
 * copy of a game is in its original's series. Multiplayer ones (2+ Players,
 * 4 Players, Co-op, Versus) come from the game data (game_data.h). Only
 * collections with a game on the card are offered.
 *
 * The player's own collections (my_collections.h) come first, newest
 * first, also while still empty.
 *
 * Which collections each game is in is worked out when the Games list loads
 * (one bit each, by entry->index), from what the game list already knows,
 * so nothing is read from the card. A game not looked up yet is in none
 * until the idle reader has read it.
 */

#ifndef COLLECTIONS_H__
#define COLLECTIONS_H__

#include <stdbool.h>
#include "menu_state.h"

/** Most collections there can be at once. */
#define COLLECTIONS_MAX     (64)

/** Work out every game's collections, for the whole Games list (shown and hidden). */
void collections_build (menu_t *menu);

/** Forget them (the list was freed). */
void collections_forget (void);

/** How many collections have games, in the order they are offered. */
int collections_count (void);

/**
 * The collection at this place in that order (0 first), as its number, or
 * -1. A collection keeps its number while places shift (the idle reader
 * learning games can add one in front).
 */
int collections_id (int place);

/** A collection's name, by number, e.g. "The Legend of Zelda" or "4 Players". */
const char *collections_name (int id);

/** The place of the player's own collection with this number (my_collections.h), or -1 for a built-in one. */
int collections_mine (int id);

/** Collection numbers run from 0 to COLLECTIONS_IDS - 1 (not all in use). */
#define COLLECTIONS_IDS     (24 + 4 + 32)

/** The number of the player's own collection at place k (my_collections.h). */
int collections_mine_id (int k);

/** True if a collection with this number exists now (offered, or the player's own). */
bool collections_exists (int id);

/** True if this entry of the Games list is in the collection with this number. */
bool collections_has (int id, const entry_t *e);

#endif /* COLLECTIONS_H__ */
