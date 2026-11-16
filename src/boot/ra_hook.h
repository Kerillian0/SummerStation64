/**
 * @file ra_hook.h
 * @brief RetroAchievements experiment, step 1 ("prove the pipe"): while a
 * game runs, copy a piece of its memory to the SummerCart64's own memory once
 * a frame, where a PC on the USB cable can read it (scripts/ra_pipe_test.py).
 *
 * It rides on the stock cheat engine (cheats.c), which already runs a few
 * instructions on every exception the game takes: ra_hook_emit() adds the
 * copy there. On a VI interrupt only (once a frame), it writes to the cart
 * at RA_HOOK_CART: "RAH1", a frame counter, then the copied words. Every
 * write waits for the PI to be idle, so it never cuts into the game's own
 * cartridge reads. The cart's memory stays writable while the game runs for
 * this (sc64_keep_rom_write). The engine sits in the Expansion Pak's memory,
 * as the stock cheats do.
 *
 * Off unless the hidden option `ra_test` is set (menu_options.h).
 */

#ifndef BOOT_RA_HOOK_H__
#define BOOT_RA_HOOK_H__

#include <stdbool.h>
#include <stdint.h>
#include "boot_io.h"

/** Where the copy goes, as the N64 sees the cart (PI); from USB it is RA_HOOK_CART - 0x10000000. */
#define RA_HOOK_CART        (0x13B00000UL)
/** At most this many words are copied a frame. */
#define RA_HOOK_MAX_WORDS   (256)

/** Copy `words` words from `rdram` (a cached 0x80xxxxxx address) every frame of the next game. */
void ra_hook_arm (uint32_t rdram, int words);

/** A copy is armed for the next game. */
bool ra_hook_armed (void);

/**
 * From cheats_install(): add the copy to the engine being built at `p`.
 * @param scratch A spare RDRAM word for the frame counter.
 * @return Where the engine carries on.
 */
io32_t *ra_hook_emit (io32_t *p, uint32_t scratch);

#endif
