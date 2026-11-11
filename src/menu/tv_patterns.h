/**
 * @file tv_patterns.h
 * @brief TV test patterns: Settings > Console > Tools > Test Patterns.
 *
 * Full-screen pictures for setting up a TV, a CRT above all:
 * - Overscan and grid: a grid of 40 px squares with a circle, and borders
 *   at the picture's edge, 2.5%, 5% (the menu stays inside this one) and
 *   10% in, each its own color, so the player sees how much the TV cuts off
 *   and whether the picture is straight and round.
 * - Color bars and grey steps: color bars at 75%, a row of grey steps from
 *   black to white, and three near-black boxes for setting brightness.
 *
 * A or right shows the next pattern, left the one before, B leaves. Each
 * pattern's name and a tip show for a few seconds, then slide away so the
 * screen is clean; Z, up or down bring them back.
 *
 * A page of Settings, as the theme editor is: Settings hands it the buttons
 * and the drawing while it is open.
 */

#ifndef TV_PATTERNS_H__
#define TV_PATTERNS_H__

#include <stdbool.h>

#include "menu_state.h"

/** Open on the first pattern. */
void tv_patterns_open (void);

/** True while a pattern is showing. */
bool tv_patterns_active (void);

/** Read this frame's buttons. */
void tv_patterns_process (menu_t *menu);

/** Draw the pattern over the whole screen. */
void tv_patterns_draw (void);

#endif /* TV_PATTERNS_H__ */
