/**
 * @file games_filter.h
 * @brief Shows only one region's games, or only 64DD disks, on the Games tab.
 *
 * C-up and C-down step through All, USA, Japan, Europe and 64DD, leaving out
 * any with no games. A game's region comes from its code in the remembered
 * game list (the last letter: E USA, J Japan, P and the other PAL letters
 * Europe), or else from tags in its file name such as (U), (J), (E), (Japan).
 * A game whose region can't be told shows under All only. Lasts while the
 * console is on.
 *
 * The games that don't match are kept at the end of the list, past
 * `menu->browser.entries`, so changing the filter reads nothing from the card.
 */

#ifndef GAMES_FILTER_H__
#define GAMES_FILTER_H__

#include <stdbool.h>
#include "menu_state.h"

/**
 * After the Games tab's list is read, before it is sorted: work out each
 * game's region and hide what the filter leaves out. `compare` is the
 * browser's own sort, used when the filter changes.
 */
void games_filter_after_load (menu_t *menu, int (*compare) (const void *, const void *));

/**
 * Step to the next (+1) or previous (-1) filter that has games, keeping the
 * selected game if it is in the new set.
 * @return false if nothing changed (no other filter has games).
 */
bool games_filter_step (menu_t *menu, int direction);

/** How many entries the list really holds, the hidden ones included (for freeing it). */
int games_filter_all_entries (int shown);

/** The filter's name for the screen, or NULL while it is All. */
const char *games_filter_label (void);

/** Forget the hidden entries (the list was freed or is another folder's). */
void games_filter_forget (void);

#endif /* GAMES_FILTER_H__ */
