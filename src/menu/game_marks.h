/**
 * @file game_marks.h
 * @brief Games the player has marked as homebrew or as a ROM hack (or as not).
 *
 * The menu tells homebrew by its game code ("ED", or no proper code at all)
 * and hacks by their names and shared codes (rom_hacks.h), but some games
 * slip through: homebrew with an ordinary code (Junk Runner 64), a hack
 * with a plain name. Z on a game marks it as homebrew (onto the Homebrew
 * tab) or as a hack (the Hack badge, the Hacks tab and step), or takes it
 * off again. A game's own mark wins over its code, a marked folder
 * (games_folders.h) and the hack finder; a game is never both.
 *
 * Kept on the card, the same for every profile (what a game is doesn't
 * change with the player): sd:/menu/homebrew.txt ("homebrew 1") and
 * sd:/menu/hackmarks.txt ("hackmarks 1"), then "+ " or "- " and a file
 * from the top of the card ("N64/Game.z64").
 */

#ifndef GAME_MARKS_H__
#define GAME_MARKS_H__

#include <stdbool.h>

/** Most games that can be marked, of each kind. */
#define GAME_MARKS_MAX  (256)

typedef enum {
    GAME_MARK_HOMEBREW,
    GAME_MARK_HACK,
    GAME_MARK_KINDS
} game_mark_kind_t;

/** A game's mark: 1 it is, -1 it isn't, 0 none (the menu decides). `name` from the top of the card. */
int game_marks_get (game_mark_kind_t kind, const char *name);

/** The same for a full path ("sd:/N64/Game.z64"). */
int game_marks_get_path (game_mark_kind_t kind, const char *path);

/**
 * Mark a game (full path): 1, -1, or 0 to forget its mark; saved at once.
 * Marking it as one kind (1) takes away a 1 of the other kind.
 * @return false if the list is full.
 */
bool game_marks_set (game_mark_kind_t kind, const char *path, int mark);

/** A game's file was renamed (file_rename.h): it keeps its marks under its new name. */
void game_marks_rename (const char *old_path, const char *new_path);

#endif /* GAME_MARKS_H__ */
