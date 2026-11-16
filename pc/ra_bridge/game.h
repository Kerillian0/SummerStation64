/**
 * @file game.h
 * @brief Which game is running: the note the menu leaves in the cart at
 * launch (src/boot/ra_hook.h, RA_HOOK_GAME), and the game's RetroAchievements
 * hash, worked out from the ROM in the cart's memory the way RetroAchievements
 * does (MD5 of the ROM in its native z64 order, as the cart holds it).
 */

#ifndef GAME_H__
#define GAME_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t rom_size;      /**< the ROM file's size, in bytes */
    char code[5];           /**< the game code, e.g. "NSME" */
    char path[244];         /**< the ROM's path on the SD card */
} game_note_t;

/** Read the menu's note. @return false if there is none (or the cart could not be read). */
bool game_read_note (game_note_t *note);

/** The ROM's RetroAchievements hash (32 hex digits + NUL), read from the cart. */
bool game_hash (const game_note_t *note, char hash[33]);

#endif
