/**
 * @file avatar_pick.h
 * @brief Choosing a profile picture: a grid of every picture there is.
 *
 * Opened by Settings > Profiles > Profile Picture, and straight after a new
 * profile is named. "None" (the player's initial) comes first, then the
 * pictures built into the menu file, then those in sd:/menu/avatars/. A
 * chooses, B leaves it as it was.
 *
 * It is a page of Settings, not a screen of its own: Settings hands it the
 * buttons and the drawing while it is open.
 */

#ifndef AVATAR_PICK_H__
#define AVATAR_PICK_H__

#include <stdbool.h>

#include "menu_state.h"

/** Open the grid for a profile, on the picture it has. */
void avatar_pick_open (int profile);

/** True while the grid is open. */
bool avatar_pick_active (void);

/** Read this frame's buttons. */
void avatar_pick_process (menu_t *menu);

/** Draw the grid over the background. */
void avatar_pick_draw (void);

#endif /* AVATAR_PICK_H__ */
