/**
 * @file hack_games.h
 * @brief Hacks that act as games of their own.
 *
 * A hack shares its original's game code, so it would take the original's
 * cover, name and description, and fold under it with Game Versions on
 * Grouped. sd:/menu/hacks.txt (written by the Companion's Hacks tab, or by
 * hand) gives such a hack a code of its own:
 *
 *     hacks 1
 *     ; size crc32 code file name
 *     16777216 0 XMLE Legend of Zelda, The Missing Link (U) (V1.3).z64
 *
 * A file is matched by its CRC-32 when Card Prep has noted one for it in
 * verified.txt (so it is found whatever it is called), otherwise by its
 * file name (without the folder, any case) and size; a CRC of 0 means
 * "match by name". From then on the menu knows the file by the new code:
 * its cover, name, description and players come from
 * sd:/menu/metadata/X/M/L/E/ like any game's, and it stays apart from its
 * original. How the game runs (save type, accessories) still comes from
 * its real code.
 */

#ifndef HACK_GAMES_H__
#define HACK_GAMES_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * If this file (a full path, "sd:/...") is a hack listed in hacks.txt, put
 * its own code in `code` (4 letters) and return true. `size` is the file's
 * size, or 0 or less if not known (then the name alone decides).
 */
bool hack_games_code (const char *path, int64_t size, char *code);

#endif /* HACK_GAMES_H__ */
