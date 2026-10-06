/**
 * @file folder_memory.h
 * @brief Remembers which entry was selected in each folder.
 *
 * Going back into a folder puts the selection where it was left, also after
 * playing a game and coming back to the menu. The most recently used folders
 * are kept in a small file on the SD card.
 */

#ifndef FOLDER_MEMORY_H__
#define FOLDER_MEMORY_H__

#include "menu_state.h"

/** Call once per frame on the Files screen, after input was handled. */
void folder_memory_update (menu_t *menu);

/**
 * Put the selection back on the remembered entry on the next update, even
 * though the folder hasn't changed. For when the list was re-sorted.
 */
void folder_memory_reselect (void);

/** Write what changed to the SD card. Call when leaving the Files screen. */
void folder_memory_flush (void);

#endif /* FOLDER_MEMORY_H__ */
