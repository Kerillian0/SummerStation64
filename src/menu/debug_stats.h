/**
 * @file debug_stats.h
 * @brief Prints free memory and frame time to the debug log.
 *
 * Read it on the PC with `localdeploy.bat /dur` (the debug session). The
 * numbers feed the memory budget table in CLAUDE.md.
 */

#ifndef DEBUG_STATS_H__
#define DEBUG_STATS_H__

#include "menu_state.h"

/** Call once per drawn frame. Prints a line every two seconds. */
void debug_stats_frame (menu_t *menu);

#endif /* DEBUG_STATS_H__ */
