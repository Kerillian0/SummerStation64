/**
 * @file game_status.h
 * @brief Status marks: None, Playing, Beaten, 100% or Backlog, set by the
 * player on each game file (Z on a cover, or Game info's Options: "Status").
 *
 * Shown as a colored badge on the cover rows' title panel; C-up / C-down on
 * the Games tab step to each status; Sort By can go by status; the Backlog
 * and Beaten collections gather them (100% counts as beaten). Feature
 * `game_status` (Games > Library > Game Status) hides all of it; the marks
 * stay on the card. Achievements will read them later.
 *
 * Per player, in profiles_file("status.txt") (game_status_list.h has the
 * format), saved at once through safe_file. The day a game is first marked
 * Beaten or 100% is kept too, for Year in Review (library_stats.h). Each file has its own (a hack's
 * isn't its original's); a file renamed in the menu keeps it.
 */

#ifndef GAME_STATUS_H__
#define GAME_STATUS_H__

#include <stdbool.h>

#include "game_status_list.h"
#include "menu_state.h"
#include "ui_components.h"

/** "Playing", "Beaten", "100%", "Backlog", or "None". */
const char *game_status_name (int status);

/** The badge's text style for a status (fonts.h). */
int game_status_style (int status);

/** A game's status, by its full path ("sd:/N64(US)/Game.z64"); GAME_STATUS_NONE with the feature off. */
game_status_t game_status_get (const char *path);

/** Set a game's status and save. */
void game_status_set (const char *path, game_status_t status);

/**
 * The games first marked Beaten or 100% in this year (a mark from before
 * the day was kept has none, and counts in no year): how many, and the
 * first `most` names, from the top of the card ("N64(US)/Game.z64"), valid
 * until a status changes.
 */
int game_status_beaten_in (int year, const char **names, int most);

/** The years games were beaten in, newest first. @return how many. */
int game_status_beaten_years (int *years, int most);

/** Goes up whenever a status changes (or the player does): lists that show them work them out again. */
int game_status_generation (void);

/**
 * The "Status" list for this game: None, Playing, Beaten, 100%, Backlog,
 * its present one marked and selected. For an Options menu row's submenu;
 * choosing one sets it.
 */
component_context_menu_t *game_status_menu (const char *path);

/** The Options row's text for this game: "Status: Beaten", "Status: None". */
const char *game_status_row_text (const char *path);

/** A game's file was renamed (file_rename.h): its status goes with it. */
void game_status_rename (const char *old_path, const char *new_path);

#endif /* GAME_STATUS_H__ */
