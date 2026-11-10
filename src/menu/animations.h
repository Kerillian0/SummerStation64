/**
 * @file animations.h
 * @brief The Animations setting: Snappy (the menu as it always was) or Showy
 * (extra movement: art fading in, covers growing into place, names sliding
 * in). Showy's animations never hold the player up: any button press or
 * move finishes the ones already running.
 */

#ifndef ANIMATIONS_H__
#define ANIMATIONS_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ANIMATIONS_SNAPPY,
    ANIMATIONS_SHOWY,
    ANIMATIONS_COUNT
} animations_t;

/** The name of a choice, for Settings. */
const char *animations_name (int choice);

/** Is Showy chosen? */
bool animations_showy (void);

/** A button was pressed or a direction moved (from actions.c): animations
 *  started before now are finished at once. */
void animations_note_press (void);

/** How far an animation started at `since` and lasting `ms` has come, from
 *  0 to 1. Always 1 on Snappy, and once a press has come after `since`. */
float animations_progress (uint64_t since, int ms);

/** Easing: fast at first, settling into place. */
float animations_ease_out (float t);

/** Easing that runs a little past the end and comes back (0 to about 1.1 to 1). */
float animations_ease_back (float t);

/** The way the selection last moved on a row of covers: 1 next, -1 previous. */
void animations_note_direction (int direction);
int animations_direction (void);

#endif
