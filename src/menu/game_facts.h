/**
 * @file game_facts.h
 * @brief A few facts about the selected game, for the badges on the Games screen.
 *
 * Looked up a moment after the selection stops moving, so scrolling stays
 * smooth. Uses the same game database and metadata the game info screen uses.
 */

#ifndef GAME_FACTS_H__
#define GAME_FACTS_H__

#include <stdbool.h>

#include "menu_state.h"

typedef struct {
    int players;            /**< number of players, 0 if not known */
    bool needs_expansion;   /**< will not run without the Expansion Pak */
    bool likes_expansion;   /**< runs better with the Expansion Pak */
    bool saves;             /**< the game keeps a save file */
    bool save_found;        /**< ...and one exists on the card */
    bool favorite;          /**< it is in the Favorites list */
} game_facts_t;

/**
 * Call once per frame on the Games screen.
 * @return The facts for the selected entry, or NULL while they are not known
 *         (still settling, or the entry is not a game).
 */
const game_facts_t *game_facts_update (menu_t *menu);

/** Forget what is known, e.g. when leaving the Games screen. */
void game_facts_reset (void);

#endif /* GAME_FACTS_H__ */
