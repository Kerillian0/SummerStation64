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

#endif /* BAKED_META_H__ */
