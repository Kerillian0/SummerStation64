/**
 * @file games_folders.h
 * @brief Which folders the Games tab shows games from.
 *
 * Always the start folder, plus any the player adds from the Folders tab
 * (Z, "Add this folder to Games"), for example one folder per region and
 * one for 64DD disks. All of them show together in one row, sorted by name.
 * The added folders are kept in sd:/menu/gamefolders.txt.
 *
 * The Games tab lists its games from the top of the card, so each game's
 * name carries its folder ("N64(JP)/Game.z64"); entry_file_name() gives
 * the part after the folder.
 */

#ifndef GAMES_FOLDERS_H__
#define GAMES_FOLDERS_H__

#include <stdbool.h>
#include "menu_state.h"

/** Most folders the player can add (the start folder comes on top). */
#define GAMES_FOLDERS_MAX   (8)

/**
 * The folders to show, start folder first, each written from the top of
 * the card ("/N64(US)"); the same folder never twice.
 * @return How many were put in `out` (room for GAMES_FOLDERS_MAX + 1).
 */
int games_folders_list (menu_t *menu, const char **out);

/** Whether a folder (a full path, "sd:/N64(JP)") is one of the added ones. */
bool games_folders_has (const char *path);

/**
 * Add or remove a folder (a full path). Saved to the card at once: this
 * happens rarely.
 * @return false if nothing changed (already there, not there, or full).
 */
bool games_folders_add (const char *path);
bool games_folders_remove (const char *path);

/** How many folders were added. */
int games_folders_count (void);

/** Forget every added folder (Settings > Library). */
void games_folders_clear (void);

/** The file's own name, without the folder in front of it. */
const char *entry_file_name (const char *name);

#endif /* GAMES_FOLDERS_H__ */
