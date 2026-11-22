/**
 * @file baked_meta.h
 * @brief Game details carried inside the menu's own file.
 *
 * The metadata pack's metadata.ini for each game (name, developer, release
 * date, players, description) can be baked into the menu with
 * assets/boxart/make_baked_meta.py. Reading it from there is quicker than
 * from the SD card, and players need no pack for the details. Where a game
 * has nothing baked, the menu reads the SD card as before.
 */

#ifndef BAKED_META_H__
#define BAKED_META_H__

#include <stdbool.h>
#include <stddef.h>

/**
 * Where this game's baked details are, if it has any.
 * @param game_code The four-letter code from the game's header.
 * @param path Receives "rom:/meta/Z/S/NZSE.ini".
 * @return true if the file is there.
 */
bool baked_meta_path (const char *game_code, char *path, size_t size);

/**
 * A game's full description: from the menu file, else the metadata pack on
 * the SD card (description.txt beside the game's metadata.ini or one folder
 * up). NULL if it has none. The text is kept until another game is asked for.
 */
const char *baked_meta_description (const char *game_code);

/**
 * A game's own full description, kept beside its file: the file its
 * metadata.ini names with `long-desc-fn` (in the game's folder), else
 * "Game.description.txt" beside "Game.z64". Wins over the pack's, since it
 * is about this one file (homebrew shares codes). NULL if there is none.
 * Looked for once per game; the text is kept until another game is asked for.
 */
const char *baked_meta_own_description (const char *rom_path, const char *long_desc_fn);

#endif /* BAKED_META_H__ */
