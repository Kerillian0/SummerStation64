/**
 * @file play_stats.h
 * @brief How many times each game has been started, and when it last was.
 *
 * Kept in a small text file on the SD card, one line per game. Games are
 * told apart by a hash of their full path, so a renamed or moved file starts
 * again from zero. Nothing is recorded while the "play_stats" feature is off.
 */

#ifndef PLAY_STATS_H__
#define PLAY_STATS_H__

#include <stdbool.h>
#include <time.h>

/** Count one start of the game at `rom_path` and save. `now` < 0 if the clock isn't working. */
void play_stats_record (const char *rom_path, time_t now);

/**
 * Look a game up.
 * @param last Set to the time of the last start, or 0 if that wasn't known.
 * @return The number of times it has been started (0 if never).
 */
int play_stats_get (const char *rom_path, time_t *last);

#endif /* PLAY_STATS_H__ */
