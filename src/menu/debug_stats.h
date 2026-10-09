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

/** Call once per drawn frame, when its work is done. Prints a line every two seconds. */
void debug_stats_frame (menu_t *menu);

#endif /* DEBUG_STATS_H__ */
