/**
 * @file save_backup.h
 * @brief Keeps earlier versions of a game's save file.
 *
 * Just before a game is started, its save as it stands is copied into
 * "backups/<game>/" beside the save, named by date and time. Up to five
 * are kept per game, the oldest making way. The same save is never kept
 * twice (each copy's name carries a check number of its contents), and a
 * blank save is never kept.
 *
 * The date is when the game was last started, which is the session that
 * save comes from; the clock's time now if that isn't known.
 *
 * The game's Options menu lists them by date under "Restore save". Going
 * back to one first keeps the save you have, so nothing is lost by it.
 *
 * Feature `save_backups` ("Back Up Saves" in Settings > Library).
 */

#ifndef SAVE_BACKUP_H__
#define SAVE_BACKUP_H__

#include "menu_state.h"
#include "ui_components.h"

#define SAVE_BACKUPS_KEPT   (5)

/** Where the game being loaded keeps its save (`out` has room for `size`). */
bool save_backup_save_path (menu_t *menu, char *out, size_t size);

/** Call when a game is about to be started, before its save is loaded. */
void save_backup_before_launch (menu_t *menu);

/**
 * Fill the "Restore save" submenu with the game's kept saves, newest
 * first. Call when the game's Options menu is opened. `list` must have
 * room for SAVE_BACKUPS_KEPT rows and the end marker.
 */
void save_backup_menu_fill (menu_t *menu, component_context_menu_t *list);

/**
 * After a row of that submenu is chosen, the game info screen calls these
 * every frame: _process() returns true while the question or its answer
 * is up (the screen must then do nothing else with the buttons), _draw()
 * shows it.
 */
bool save_backup_prompt_process (menu_t *menu);
void save_backup_prompt_draw (void);

#endif /* SAVE_BACKUP_H__ */
