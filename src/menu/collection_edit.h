/**
 * @file collection_edit.h
 * @brief Putting games in the player's own collections, and making,
 *        renaming and deleting them (my_collections.h), from any cover tab.
 *
 * "Add to collection..." opens a list of the player's collections, newest
 * first, with a tick on those the game is in: A puts it in or takes it
 * out (the list stays open), B closes it. Its first row, "New collection",
 * spells a name with the letter dial (letter_wheel_name_begin()) and puts
 * the game in it. Deleting asks first. After a change the Collections tab
 * is worked out again (games_filter_collections_changed()).
 */

#ifndef COLLECTION_EDIT_H__
#define COLLECTION_EDIT_H__

#include <stdbool.h>
#include "menu_state.h"

/** "Add to collection..." for the game at this full path ("sd:/..."). */
void collection_edit_pick (const char *path);

/** Make a new, empty collection (spelled with the dial). */
void collection_edit_new (void);

/** Rename the player's collection `k` (my_collections.h). */
void collection_edit_rename (int k);

/** Delete the player's collection `k`, asking first. */
void collection_edit_delete (int k);

/** Take the game at this full path out of the player's collection `k`. */
void collection_edit_remove (menu_t *menu, int k, const char *path);

/** While any of these is showing, it has the controls: true if it took this frame's. */
bool collection_edit_process (menu_t *menu);

/** Draw whichever is showing (the letter dial draws itself: letter_wheel_draw()). */
void collection_edit_draw (void);

#endif /* COLLECTION_EDIT_H__ */
