/**
 * @file ra_web.h
 * @brief The RetroAchievements web API, for reading only: log in (once; the
 * token is kept beside the .exe in ra_bridge_login.txt, the password never),
 * find a game by its hash, download its achievement definitions. Nothing is
 * ever sent to the player's account: no session, no unlocks (the owner's
 * choice until RetroAchievements approves the bridge).
 */

#ifndef RA_WEB_H__
#define RA_WEB_H__

#include <stdbool.h>
#include <stdint.h>

#include "rcheevos/include/rc_api_runtime.h"

/** Log in: the saved token, else ask for the username and password once. */
bool ra_web_login (void);

/** The logged-in player's name. */
const char *ra_web_user (void);

/** The game's RetroAchievements ID from its hash (0 if unknown to them). */
bool ra_web_resolve_hash (const char *hash, uint32_t *game_id);

/** Download the game's achievements; free with rc_api_destroy_fetch_game_data_response(). */
bool ra_web_fetch_game (uint32_t game_id, rc_api_fetch_game_data_response_t *game);

/** What went wrong last. */
const char *ra_web_error (void);

#endif
