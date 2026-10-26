/**
 * @file my_collections.h
 * @brief Collections the player makes: a name and a list of games.
 *
 * Kept in sd:/menu/collections.txt, which the Companion can also edit:
 *
 *     collections 1
 *     = Couch Coop
 *     N64(US)/Mario Kart 64 (U) [!].z64
 *     N64(US)/Mario Party 2 (U) [!].z64
 *     = Retro Night
 *     ...
 *
 * Each game is its file from the top of the card, so a renamed or moved
 * file leaves the collection. Up to MY_COLLECTIONS_MAX collections of
 * MY_COLLECTIONS_GAMES games. Saved straight after each change.
 */

#ifndef MY_COLLECTIONS_H__
#define MY_COLLECTIONS_H__

#include <stdbool.h>
#include <stdint.h>

#define MY_COLLECTIONS_MAX      (32)
#define MY_COLLECTIONS_GAMES    (128)
#define MY_COLLECTIONS_NAME     (28)    /**< longest name, in bytes */

/** How many collections the player has. */
int my_collections_count (void);

/** A collection's name ("" if there is none at `k`). */
const char *my_collections_name (int k);

/** How many games are in collection `k`. */
int my_collections_games (int k);

/**
 * Make a new collection, at the end of the list (the others keep their
 * places; the Collections tab shows the newest first).
 * @return its place, or -1 if there are MY_COLLECTIONS_MAX already.
 */
int my_collections_create (const char *name);

/** Rename collection `k`. */
void my_collections_rename (int k, const char *name);

/** Delete collection `k` (the games themselves are untouched). */
void my_collections_delete (int k);

/** True if the game (full path, "sd:/...") is in collection `k`. */
bool my_collections_has (int k, const char *path);

/**
 * Put the game in collection `k`, or take it out.
 * @return false if it could not be added (the collection is full).
 */
bool my_collections_set (int k, const char *path, bool in);

/** Which collections the game is in: bit k for collection k. */
uint32_t my_collections_of (const char *path);

/** A game's file was renamed (file_rename.h): it stays in its collections. */
void my_collections_rename_game (const char *old_path, const char *new_path);

#endif /* MY_COLLECTIONS_H__ */
