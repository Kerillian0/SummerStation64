/**
 * @file library_stats.h
 * @brief Library Stats (Settings > Games > Saves & Play): one page about the
 * games on the card.
 *
 * Tiles along the top (games, their total size, time played, SD card free
 * space), the five most played in a big frame, the most common developers
 * and genres as bars beside it, and how many carry each status mark
 * (game_status.h) under it.
 *
 * The Games tab's folders are read here, a few milliseconds a frame (the
 * list in memory belongs to whichever tab was open last): the start folder,
 * the added folders and one level under those, as the Games tab reads them.
 * Cartridges and 64DD disks count; hidden games and Retro games don't. What
 * each game is comes from the game list in memory (game_index.h), so nothing
 * but the folders is read. The free space is asked for once per power-on.
 */

#ifndef LIBRARY_STATS_H__
#define LIBRARY_STATS_H__

#include <stdbool.h>
#include "menu_state.h"

/** Open the page and start counting. */
void library_stats_open (menu_t *menu);

/** True while the page is showing: Settings hands it its buttons and its frame. */
bool library_stats_active (void);

/** Read the buttons (B leaves) and count a little more. */
void library_stats_process (menu_t *menu);

/** Draw the page (the background is already drawn). */
void library_stats_draw (void);

#endif /* LIBRARY_STATS_H__ */
