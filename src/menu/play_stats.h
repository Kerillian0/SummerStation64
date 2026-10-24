/**
 * @file play_stats.h
 * @brief How many times each game has been started, when it last was, and
 *        for how long it has been played.
 *
 * Kept in a small text file on the SD card, one line per game. Games are
 * told apart by a hash of their full path, so a renamed or moved file starts
 * again from zero. Nothing is recorded while the "play_stats" feature is off.
 *
 * Time played: nothing of the menu runs during a game, so a session is
 * noted when the game starts and settled when the menu next starts. After
 * RESET it ran until then. After the power switch the time the console was
 * off would count too, so it ends when the cart last wrote the game's save
 * to the card instead, if that was after the start; otherwise (no save, or
 * not written) it isn't counted. A session over 12 hours is taken for a
 * power-off as well (with the USB cable in, the cart stays powered and a
 * power-off looks like RESET).
 */

#ifndef PLAY_STATS_H__
#define PLAY_STATS_H__

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * Count one start of the game at `rom_path`, note the session begun (with
 * the game's save file, `save_path`), and save. `now` < 0 if the clock
 * isn't working (then no session is noted).
 */
void play_stats_record (const char *rom_path, const char *save_path, time_t now);

/**
 * Settle the session noted at the last game start, if there is one: add
 * its length to that game's time played. Call once when the menu starts.
 * @param power_on The menu starts after the power switch, not RESET.
 */
void play_stats_settle (bool power_on);

/** The time a game has been played, in seconds (0 if none was counted). */
uint32_t play_stats_seconds (const char *rom_path);

/**
 * Look a game up.
 * @param last Set to the time of the last start, or 0 if that wasn't known.
 * @return The number of times it has been started (0 if never).
 */
int play_stats_get (const char *rom_path, time_t *last);

#endif /* PLAY_STATS_H__ */
