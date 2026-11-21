/**
 * @file cover_pick.h
 * @brief Z > "Cover: USA": choose which region's copy gives a game its cover.
 *
 * With Region Copies on One Cover or Fixed Cover, a game kept in several
 * regions shows once. On such a cover the Options menu (Z) has a "Cover"
 * row naming the region shown; it opens a list of Automatic and each region
 * the game has. A region chosen there stays the game's cover whatever is
 * played and whatever Preferred Region says (kept by region_pick.c in
 * regionpicks.txt); Automatic goes back to the usual rule.
 */

#ifndef COVER_PICK_H__
#define COVER_PICK_H__

#include <stdbool.h>
#include "menu_state.h"
#include "ui_components.h"

/**
 * Get the row ready for this cover. False when it doesn't stand for copies
 * from two regions or more (then there is no row).
 * @param after called once a choice is saved: read the list again, then
 *              cover_pick_select().
 */
bool cover_pick_offer (menu_t *menu, entry_t *cover, void (*after) (menu_t *menu));

/** The row's text, "Cover: Europe" (after cover_pick_offer()). */
const char *cover_pick_row_text (void);

/** The list the row opens (after cover_pick_offer()). */
component_context_menu_t *cover_pick_menu (void);

/** After the list was read again: select the game's cover, whichever copy it is now. */
void cover_pick_select (menu_t *menu);

/** A short note saying what was chosen. */
const char *cover_pick_note (void);

#endif /* COVER_PICK_H__ */
