/**
 * @file watch.h
 * @brief Checks a game's achievements against every frame the hook copies.
 *
 * The achievements go into rcheevos's runtime. What memory they read is found
 * by asking: a frame is run with nothing copied yet and every address the
 * runtime reads is noted; those are packed into the hook's list (at most
 * HOOK_MAX_RANGES ranges, HOOK_MAX_BYTES bytes). A later frame that reads
 * something not copied (an achievement following a pointer) gets zeros
 * there, and the list is planned again from what that frame read.
 *
 * Byte order: the N64 is big-endian. RetroAchievements' N64 sets may address
 * memory as emulators expose it, 32-bit words byte-swapped (byte A of their
 * view is the console's byte A ^ 3); WATCH_VIEW_SWAPPED gives that view,
 * WATCH_VIEW_NATIVE the console's own.
 */

#ifndef WATCH_H__
#define WATCH_H__

#include <stdbool.h>
#include <stdint.h>

#include "rcheevos/include/rc_api_runtime.h"

typedef enum { WATCH_VIEW_SWAPPED, WATCH_VIEW_NATIVE } watch_view_t;

/** Load the game's promoted achievements into the checker. */
int watch_load (const rc_api_fetch_game_data_response_t *game, watch_view_t view);

/** Find what memory they read and give the hook its list. */
bool watch_plan (void);

/** Check one frame (from hook_poll). Prints "WOULD UNLOCK" for each achievement it meets. */
void watch_frame (uint32_t frame, const uint8_t *data, uint32_t length, void *context);

/** Frames that read memory not copied yet (each sends the hook a new list). */
uint32_t watch_misses (void);

/** Achievements left out because they follow pointers (not checked yet). */
int watch_skipped (void);

/** Achievements met so far. */
int watch_met (void);

/** Ranges and bytes in the hook's current list. */
void watch_list_size (int *ranges, int *bytes);

#endif
