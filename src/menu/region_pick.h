/**
 * @file region_pick.h
 * @brief Region Copies: which region's copy of a game gives its cover.
 *
 * With Region Copies on One Cover (Settings > Library), a game kept in
 * several regions shows once under All. Its cover is the copy from the
 * Preferred Region, unless the player has started another region's copy of
 * that game three times in a row: then that region becomes the game's own
 * choice. Starting the preferred region's copy once, or changing Preferred
 * Region, goes back to the preferred one. A game is the first three letters
 * of its code ("NSM" for NSME, NSMJ and NSMP).
 *
 * The games' own choices are kept in sd:/menu/regionpicks.txt.
 */

#ifndef REGION_PICK_H__
#define REGION_PICK_H__

#include <stdbool.h>
#include "menu_state.h"

/** Region Copies (option `region_copies`). */
typedef enum {
    REGION_COPIES_SEPARATE,     /**< every copy its own cover (the default) */
    REGION_COPIES_ONE_COVER,    /**< one cover per game under All */
    REGION_COPIES_COUNT
} region_copies_t;

/** Preferred Region (option `preferred_region`). */
typedef enum {
    PREFERRED_USA,
    PREFERRED_JAPAN,
    PREFERRED_EUROPE,
    PREFERRED_COUNT
} preferred_region_t;

/** Names for Settings. */
const char *region_pick_copies_name (int choice);
const char *region_pick_preferred_name (int choice);

/**
 * How well a copy with this code suits the cover of its game: 2 for the
 * game's own choice, 1 for the preferred region, 0 otherwise. A code for
 * every region ('A') suits either.
 */
int region_pick_score (const char *code);

/**
 * A game is being started: count it towards that game's own choice. Only
 * while Region Copies is on One Cover and the Games list holds copies of
 * the game from more than one region. Saves when something changed.
 */
void region_pick_launched (menu_t *menu, const char *rom_path, const char *code);

/** Forget every game's own choice (Preferred Region was changed). */
void region_pick_forget_all (void);

#endif /* REGION_PICK_H__ */
