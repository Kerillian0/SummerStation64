/**
 * @file screen_dim.h
 * @brief Dim Screen: left alone for the time chosen in Settings > Console >
 * TV & Video > Dim Screen, the whole picture fades down to a fifth of its
 * brightness and the menu's music goes quieter, on every screen, so nothing
 * bright sits still on a CRT. With Attract Mode on as well, the covers start
 * drifting first and the dim comes later (both count from the last press).
 *
 * The first press only wakes it. The TV test patterns are never dimmed.
 * Drawn last in every frame: the link wraps rdpq_detach_show() (Makefile).
 */

#ifndef SCREEN_DIM_H__
#define SCREEN_DIM_H__

#include <stdbool.h>

/** Values of option `screen_dim`: how long the menu waits first. */
typedef enum {
    SCREEN_DIM_OFF,
    SCREEN_DIM_5_MIN,
    SCREEN_DIM_10_MIN,
    SCREEN_DIM_20_MIN,
    SCREEN_DIM_COUNT
} screen_dim_wait_t;

/** From actions_update(): a press while the screen is dimmed only wakes it.
 *  @return true if this frame's press was taken for that (clear the actions). */
bool screen_dim_wake (void);

/** The screen is dimmed (or dimming). */
bool screen_dim_active (void);

/** Name of an option `screen_dim` value, for Settings. */
const char *screen_dim_name (int value);

#endif
