/**
 * @file display_name.h
 * @brief How an entry's name is shown on screen (the file itself is untouched).
 */

#ifndef DISPLAY_NAME_H__
#define DISPLAY_NAME_H__

#include "menu_state.h"

/**
 * The name to show for a list entry, tidied according to the player's
 * settings: without the file extension ("hide_extensions"), without the
 * region and version tags in brackets ("hide_tags"), and with a trailing
 * article moved to the front ("tidy_titles"), e.g.
 * "Legend of Zelda, The - Ocarina of Time.z64" becomes
 * "The Legend of Zelda - Ocarina of Time".
 *
 * Only games (ROMs, disks, emulator ROMs) are changed. The result is valid
 * until the next call.
 */
const char *display_name (const entry_t *entry);

/**
 * The same tidying for a bare file name that is known to be a game, as on
 * the History and Favorites lists. Valid until the next call.
 */
const char *display_name_file (const char *filename);

/**
 * For the game info screens: the tidied name, or the real file name while
 * the player has asked to see it. Valid until the next call.
 */
const char *display_name_info (const char *filename);

/**
 * Switch the game info screens between the tidied and the real file name.
 * Shaped as a context menu action so it can go straight into an options list.
 */
void display_name_toggle_real (menu_t *menu, void *arg);

#endif /* DISPLAY_NAME_H__ */
