/**
 * @file play_stats.h
 * @brief How many times each game has been started, when it last was, and
 *        for how long it has been played.
 *
 * Kept in a small text file on the SD card, one line per game, then a line
 * per game and year (play_years.h). Games are told apart by a hash of their
 * full path; renaming in the menu carries the counts over
 * (play_stats_rename()), a file moved on a PC starts again from zero. Nothing is recorded while the "play_stats" feature is off.
 *
 * Time played: nothing of the menu runs during a game, so a session is
 * noted when the game starts and settled when the menu next starts. After
 * RESET it ran until then (up to 12 hours). After the power switch the time
 * the console was off counts too, and the cart doesn't date its save writes
 * (seen on hardware), so such a session only counts when the console was
 * back on within 4 hours of the start (the owner's choice): a short
 * switch-off adds a little, an overnight one adds nothing. With the USB
 * cable in, the cart stays powered and a power-off looks like RESET.
 */

#ifndef PLAY_STATS_H__
#define PLAY_STATS_H__

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * Count one start of the game at `rom_path`, note the session begun, and
 * save. `now` < 0 if the clock isn't working (then no session is noted).
 */
void play_stats_record (const char *rom_path, time_t now);

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

/**
 * The session settled when the menu started (play_stats_settle()), if it
 * was counted: the game's path, how long it lasted, and that game's starts
 * and time played since. For the assistant's "welcome back".
 * @return False if no session was settled at this start.
 */
bool play_stats_settled (const char **rom_path, uint32_t *length, uint32_t *count, uint32_t *seconds);

/** All time played, in seconds, and how many different games have been started. */
void play_stats_totals (uint32_t *seconds, int *games);

/** A game's file was renamed (file_rename.h): its counts go with it (its years too). */
void play_stats_rename (const char *old_path, const char *new_path);

/*
 * Per year (play_years.h): starts are counted in the year of the start, a
 * session's time in the year it was started in. Kept since 2026-11-12;
 * earlier play shows in the totals only. Nothing is counted without a
 * working clock.
 */

/** The years with anything played, newest first. @return how many (at most `max`). */
int play_stats_years (int *list, int max);

/** A game's starts and time played in one year. @return false if it wasn't played then. */
bool play_stats_year_get (const char *rom_path, int year, uint32_t *starts, uint32_t *seconds);

/** One year's time played, starts, and how many different games were started. */
void play_stats_year_totals (int year, uint32_t *seconds, uint32_t *starts, int *games);

#endif /* PLAY_STATS_H__ */
