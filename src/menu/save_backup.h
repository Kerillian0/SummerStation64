/**
 * @file save_backup.h
 * @brief Keeps spare copies of a game's save file.
 *
 * Just before a game is started, its save file as it stands is copied to a
 * "backups" folder beside it, as "Game.1.sav". The three most recent
 * copies are kept (1 is the newest); a copy is only made when the save has
 * changed since the last one, and never of an empty save. So if a game, a
 * power cut or a bad write spoils a save, the one from before that session
 * is still on the card.
 *
 * Feature `save_backups` ("Back Up Saves" in Settings > Library).
 */

#ifndef SAVE_BACKUP_H__
#define SAVE_BACKUP_H__

#include "menu_state.h"

/** Call when a game is about to be started, before its save is loaded. */
void save_backup_before_launch (menu_t *menu);

#endif /* SAVE_BACKUP_H__ */
