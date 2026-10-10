/**
 * @file debug_stats.h
 * @brief Prints free memory and frame time to the debug log.
 *
 * Read it on the PC with `localdeploy.bat /d` or `deploy-sd.bat /d`. The
 * numbers feed the memory budget table in CLAUDE.md.
 */

#ifndef DEBUG_STATS_H__
#define DEBUG_STATS_H__

#include "menu_state.h"

/** Call when work on a frame starts (a screen buffer has been handed out). */
void debug_stats_begin (void);

/** Call once per drawn frame, after frame_rate_end_frame(). Prints a line every two seconds. */
void debug_stats_frame (menu_t *menu);

/**
 * Draw the performance overlay (option `perf_overlay`, "Performance Overlay"
 * in Settings > System): frame time, how long the graphics chip took, and
 * free memory, over the last half second. Call at the end of a screen's
 * drawing, just before the frame is shown.
 */
void debug_stats_overlay_draw (void);

/** Names of the overlay's two choices, for the settings screen. */
const char *debug_stats_overlay_name (int choice);

#endif /* DEBUG_STATS_H__ */
