/**
 * @file homebrew_marks.h
 * @brief Games the player has marked as homebrew, or as not homebrew.
 *
 * The menu tells homebrew by its game code ("ED", or no proper code at all),
 * but some homebrew carries an ordinary code (Junk Runner 64) and so landed
 * on the Games tab. Z on a game marks it as homebrew (onto the Homebrew tab)
 * or as not homebrew (back to the Games tab). A game's own mark wins over its
 * code, a marked folder (games_folders.h) and the hack finder.
 *
 * Kept in sd:/menu/homebrew.txt, the same for every profile: "homebrew 1",
 * then "+ " or "- " and a file from the top of the card ("N64/Game.z64").
 */

#ifndef HOMEBREW_MARKS_H__
#define HOMEBREW_MARKS_H__

#include <stdbool.h>

/** Most games that can be marked. */
#define HOMEBREW_MARKS_MAX  (256)

/** A game's mark: 1 homebrew, -1 not homebrew, 0 none (its code decides). `name` from the top of the card. */
int homebrew_marks_get (const char *name);

/** The same for a full path ("sd:/N64/Game.z64"). */
int homebrew_marks_get_path (const char *path);

/** Mark a game (full path): 1, -1, or 0 to forget its mark; saved at once. @return false if the list is full. */
bool homebrew_marks_set (const char *path, int mark);

/** A game's file was renamed (file_rename.h): it keeps its mark under its new name. */
void homebrew_marks_rename (const char *old_path, const char *new_path);

#endif /* HOMEBREW_MARKS_H__ */
