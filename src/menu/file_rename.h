/**
 * @file file_rename.h
 * @brief Rename a file from the Folders tab (Z, "Rename...").
 *
 * The name is spelled with the letter dial's file-name mode
 * (letter_wheel_file_name_begin()), starting from the file's own name; its
 * ending (".z64") stays. Everything that goes by the file's name moves with
 * it: beside it, its save, per-game settings, cheats and details
 * (Game.sav, .ini, .meta, .metadata.ini, .datel, .datel.txt); its save in
 * the saves folder and its kept saves (save_backup.h); a Retro game's art
 * folder; and the menu's own lists: play counts, the game list, favorites
 * and recent, the player's collections, hidden games, 64DD pairs and Card
 * Prep's check mark. A name already taken in the folder is refused and the
 * name stays open to change.
 */

#ifndef FILE_RENAME_H__
#define FILE_RENAME_H__

#include <stdbool.h>
#include "menu_state.h"

/** Start renaming the file at this full path ("sd:/N64/Game.z64"), of this size. */
void file_rename_begin (menu_t *menu, const char *path, int64_t size);

/** While the name is being spelled it has the controls: true if it took this frame's. */
bool file_rename_process (menu_t *menu);

/** Draw the dial while the name is being spelled. */
void file_rename_draw (void);

#endif /* FILE_RENAME_H__ */
