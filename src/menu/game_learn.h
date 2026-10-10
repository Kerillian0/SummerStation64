/**
 * @file game_learn.h
 * @brief Gets to know the Games tab's games while the player is idle.
 *
 * The remembered game list (game_index) learns a game's code when its
 * cover is first shown. This reads the rest: while the Games tab sits still
 * for a moment, it reads one unknown game's header per frame (15-20 ms
 * each, so a frame now and then takes longer while nothing is moving) and
 * notes its code. The region filter and the covers then know those games
 * before they are visited. Any movement pauses it.
 */

#ifndef GAME_LEARN_H__
#define GAME_LEARN_H__

#include <stdbool.h>
#include "menu_state.h"

/** Call once a frame on the Games tab, after the frame has been handed over. */
void game_learn_update (menu_t *menu);

/** True while games are still being read (the list is saved once it is done). */
bool game_learn_busy (void);

#endif /* GAME_LEARN_H__ */
