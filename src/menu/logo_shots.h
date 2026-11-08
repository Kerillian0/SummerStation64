/**
 * @file logo_shots.h
 * @brief Test build only (FLAGS=-DLOGO_SHOTS, see tests/logo_shots): Game
 *        info for every game in a list, each finished screen sent out
 *        through the emulator's debug log.
 */

#ifndef LOGO_SHOTS_H__
#define LOGO_SHOTS_H__

#include <libdragon.h>

#include "menu_state.h"

/** Call once a frame, after it has been drawn (menu.c). */
void logo_shots_after_frame (menu_t *menu, surface_t *display);

#endif /* LOGO_SHOTS_H__ */
