/**
 * @file ra_hook.h
 * @brief RetroAchievements experiment (step 2a): while a game runs, copy the
 * parts of its memory a PC asks for to the SummerCart64's own memory, once a
 * frame, where the PC reads them over the USB cable (scripts/ra_pipe_test.py,
 * later the bridge program).
 *
 * It rides on the stock cheat engine (cheats.c), which already runs on every
 * exception the game takes: ra_hook_emit() adds a small loop there that runs
 * on VI interrupts only (once a frame). Cart memory (as the N64 sees it; from
 * USB subtract 0x10000000):
 *
 *   RA_HOOK_LIST    0x13B00000  written by the PC: "RAL1", a count (at most
 *                               RA_HOOK_MAX_RANGES), then per range the RDRAM
 *                               address (0x00000000-0x007FFFFF) and its length
 *                               in bytes (a multiple of 4). Write "RAL1" last.
 *   RA_HOOK_STATUS  0x13B01000  written by the hook every frame: "RAH2", then
 *                               the frame counter.
 *   RA_HOOK_GAME    0x13B02000  written by the menu at launch (ra_hook_note_game):
 *                               "RAG1", the ROM file's size, its game code, then
 *                               its path (NUL-terminated, at most 243 bytes), so
 *                               the PC can hash the ROM straight from the cart.
 *   RA_HOOK_RING    0x13B10000  RA_HOOK_SLOTS slots of RA_HOOK_SLOT_SIZE bytes;
 *                               frame N goes to slot N % RA_HOOK_SLOTS: the
 *                               ranges' bytes from offset 4 in list order (at
 *                               most RA_HOOK_SLOT_SIZE - 4), then the frame
 *                               number at offset 0, written last, so a slot
 *                               holding the expected number is complete.
 *
 * Every cart access waits for the PI to be idle, so it never cuts into the
 * game's own cartridge reads. The cart's memory stays writable while the game
 * runs for this (sc64_keep_rom_write). The engine sits in the Expansion Pak's
 * memory, as the stock cheats do. Off unless the hidden option `ra_test` is
 * set (menu_options.h).
 */

#ifndef BOOT_RA_HOOK_H__
#define BOOT_RA_HOOK_H__

#include <stdbool.h>
#include <stdint.h>
#include "boot_io.h"

#define RA_HOOK_LIST        (0x13B00000UL)
#define RA_HOOK_GAME        (0x13B02000UL)  /* written by the menu at launch: "RAG1", ROM size, game code, ROM path */
#define RA_HOOK_STATUS      (0x13B01000UL)
#define RA_HOOK_RING        (0x13B10000UL)
#define RA_HOOK_SLOTS       (16)
#define RA_HOOK_SLOT_SIZE   (4096)
#define RA_HOOK_MAX_RANGES  (31)

/** Install the copy loop with the next game (or not). */
void ra_hook_arm (bool on);

/**
 * From the menu at launch: clear the old copy list and status, and note the
 * game for the PC (its ROM file's size, game code and path). The cart's
 * memory must still be writable (it is, until the menu hands over).
 */
void ra_hook_note_game (const char *path, uint32_t rom_size, const char game_code[4]);

/** The copy loop is armed for the next game. */
bool ra_hook_armed (void);

/**
 * From cheats_install(): add the copy loop to the engine being built at `p`.
 * @param scratch 64 spare bytes of RDRAM (registers are saved there; frame counter).
 * @return Where the engine carries on.
 */
io32_t *ra_hook_emit (io32_t *p, uint32_t scratch);

#endif
