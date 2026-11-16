/**
 * @file watch.h
 * @brief Checks a game's achievements against every frame the hook copies.
 *
 * The achievements go into rcheevos's runtime, and what memory they read is
 * taken from its parsed form: every fixed address goes into the hook's list
 * as ranges; every pointer chain (AddAddress) becomes entries the hook
 * follows itself each frame, copying what the chain leads to and where that
 * was, so each frame is checked with that frame's own memory. An achievement
 * with a chain of a kind the hook can't follow is left out ("not checked
 * yet"). A frame that still reads something not copied gets zeros there and
 * is counted (watch_misses).
 *
 * Byte order: the N64 is big-endian. RetroAchievements' N64 sets address
 * memory as emulators expose it, 32-bit words byte-swapped (byte A of their
 * view is the console's byte A ^ 3); WATCH_VIEW_SWAPPED gives that view,
 * WATCH_VIEW_NATIVE the console's own (pointer chains: swapped view only).
 */

#ifndef WATCH_H__
#define WATCH_H__

#include <stdbool.h>
#include <stdint.h>

#include "hook_ring.h"
#include "rcheevos/include/rc_api_runtime.h"

typedef enum { WATCH_VIEW_SWAPPED, WATCH_VIEW_NATIVE } watch_view_t;

/** Load the game's promoted achievements into the checker. */
int watch_load (const rc_api_fetch_game_data_response_t *game, watch_view_t view);

/** Work out what memory they read and give the hook its list. */
bool watch_plan (void);

/** Check one frame (from hook_poll). Prints "WOULD UNLOCK" for each achievement it meets. */
void watch_frame (uint32_t frame, const uint8_t *data, uint32_t length, void *context);

/** Frames that read memory the hook didn't copy (should stay 0). */
uint32_t watch_misses (void);

/** Achievements left out because they follow pointers in a way the hook can't (not checked yet). */
int watch_skipped (void);

/** Achievements met so far. */
int watch_met (void);

/** Fixed ranges, pointer chains and bytes a frame in the hook's list. */
void watch_list_size (int *ranges, int *chains, int *bytes);

/** The hook's list as planned (for tests). */
const hook_entry_t *watch_list (int *count);

#endif
