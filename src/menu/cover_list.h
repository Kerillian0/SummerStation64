/**
 * @file cover_list.h
 * @brief Where the cover row gets its games from.
 *
 * The cover row, its box art and its badges were written for the open
 * folder. This lets them show another list instead (the Recent tab), by
 * asking here for the entries, the selected one and each file's full path.
 */

#ifndef COVER_LIST_H__
#define COVER_LIST_H__

#include "menu_state.h"
#include "path.h"

typedef struct {
    entry_t *list;      /**< the entries: name and kind of each */
    int entries;        /**< how many */
    int selected;       /**< which one is in the middle */
    /** Full path of entry `index`. The caller frees it. */
    path_t *(*path) (menu_t *menu, int index);
} cover_list_t;

/** Show covers from `list` from now on. NULL goes back to the open folder. */
void cover_list_use (const cover_list_t *list);

/** The list the covers come from right now. */
const cover_list_t *cover_list_current (menu_t *menu);

/**
 * Which entry sits `offset` places from the selected one (-1 is the cover
 * to its left, +1 to its right), or -1 if nothing is shown there.
 *
 * With "Wrap File List" on the row is a ring: left of the first game is the
 * last one. A place is only filled that way while the list is long enough
 * that the same game cannot show on both sides at once.
 */
int cover_list_neighbor (menu_t *menu, int offset);

#endif /* COVER_LIST_H__ */
