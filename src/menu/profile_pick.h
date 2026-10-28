/**
 * @file profile_pick.h
 * @brief "Who's playing?": choose a profile at power-on.
 *
 * With two or more profiles (profiles.h), the menu asks who is playing
 * after the intro whenever the console has been switched on (not after
 * RESET, when the same player is most likely still there). The player who
 * played last is selected; A plays as the one chosen. The first tab then
 * fades in as usual.
 */

#ifndef PROFILE_PICK_H__
#define PROFILE_PICK_H__

#include <stdbool.h>

#include "menu_state.h"

/** Call from the startup screen after intro_begin(): shows the picker if it is wanted. */
void profile_pick_begin (menu_t *menu);

/**
 * Draw one frame of the picker and handle its buttons.
 * @return false if the picker is not showing (the caller draws as usual).
 */
bool profile_pick_display (menu_t *menu, surface_t *display);

#endif /* PROFILE_PICK_H__ */
