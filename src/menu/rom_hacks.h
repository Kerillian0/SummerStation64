/**
 * @file rom_hacks.h
 * @brief Tells which games in a list are hacks of other games.
 *
 * A hack is a changed copy of a game: it carries the original's game code,
 * so the menu would otherwise take it for the original (same box art, same
 * date, same badges). Two ways of telling, both from what is already known
 * without reading the card:
 *
 * 1. The file name says so: "hack", "romhack", "redux", or a GoodN64
 *    "[h1]" style tag. ("Patched" alone is not enough: translations and
 *    fixes are patched too, such as the owner's Dezaemon 3D.)
 * 2. Several files carry the same game code. Their names are compared
 *    word by word, leaving out tags in brackets and little words: a hack's
 *    name holds all of the original's words and adds its own ("Super
 *    Mario 64 - Return to Yoshi's Island"), while another copy of the game
 *    has the same words in any order ("007 - GoldenEye", "GoldenEye 007
 *    (U) [!]"). The name whose words turn up in most of the others is the
 *    original; files with the same words as it are copies of it, and the
 *    rest are hacks. When no name holds another (Pokemon Stadium and the
 *    Snap Station kiosk share a code), nothing is called a hack.
 *
 * Two earlier tries failed on the owner's library (2026-10-14): "the less
 * tidy name of two" called second copies of games hacks, and "the larger
 * file" did too (a 16 MB GoldenEye beside the 12 MB one) and took Ocarina
 * of Time for a hack of The Missing Link, which is smaller.
 *
 * This file has no menu or console code in it, so the PC can test it.
 */

#ifndef ROM_HACKS_H__
#define ROM_HACKS_H__

#include <stdbool.h>

/** True if the file name itself says the game is a hack (rule 1). */
bool rom_hacks_name_says (const char *file_name);

/**
 * Mark the hacks in a list.
 * @param names Each game's file name, without its folder.
 * @param codes Each game's four-letter code, or four zero bytes if not known.
 * @param is_rom Whether each entry is a cartridge game at all.
 * @param hack Receives true for each hack.
 */
void rom_hacks_find (const char *const *names, const char (*codes)[4], const bool *is_rom, int count, bool *hack);

#endif /* ROM_HACKS_H__ */
