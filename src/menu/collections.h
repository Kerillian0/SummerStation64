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
#define COLLECTIONS_MAX     (32)

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

/** True if this entry of the Games list is in the collection with this number. */
bool collections_has (int id, const entry_t *e);

#endif /* COLLECTIONS_H__ */
