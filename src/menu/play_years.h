/**
 * @file play_years.h
 * @brief Play stats per year: for each game and year, how many times it was
 * started and for how long it was played that year. The groundwork for Year
 * in Review; play_stats.c keeps the running totals and uses this beside them.
 *
 * In playstats.txt each is a line of its own, "y <game hash> <year> <starts>
 * <seconds>". Files from before have none (years count from then), and older
 * menu builds skip these lines. No console code here, so it is tested on the
 * PC (tests/play_years_test.c).
 */

#ifndef PLAY_YEARS_H__
#define PLAY_YEARS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** One game's year. 12 bytes. */
typedef struct {
    uint32_t key;       /**< the game: play_stats' hash of its full path */
    uint16_t year;      /**< e.g. 2026 */
    uint16_t starts;    /**< times started that year */
    uint32_t seconds;   /**< time played that year */
} play_year_t;

/** A table of them, in memory the caller gives. */
typedef struct {
    play_year_t *rows;
    int count;
    int max;
} play_years_t;

/** Read one line. @return false if it isn't a year line (or is damaged). */
bool play_years_parse (const char *line, play_year_t *out);

/** Write one line, with its line end. @return its length (as snprintf). */
int play_years_format (const play_year_t *row, char *out, size_t size);

/** The row for this game and year, or NULL. */
play_year_t *play_years_find (play_years_t *t, uint32_t key, int year);

/**
 * The row for this game and year, made if there is none. When the table is
 * full, the row of the earliest year with the least time played makes way.
 * @return NULL only if the table has no room at all (max 0).
 */
play_year_t *play_years_add (play_years_t *t, uint32_t key, int year);

/** Take a row in as read from the file (a second line for the same game and year adds to the first). */
void play_years_load_row (play_years_t *t, const play_year_t *row);

/** Forget every year of this game (its stats entry went to another game). */
void play_years_forget (play_years_t *t, uint32_t key);

/** Every year of game `from` now belongs to `to` (the file was renamed). */
void play_years_rekey (play_years_t *t, uint32_t from, uint32_t to);

/** The years that have any rows, newest first. @return how many (at most `max`). */
int play_years_list (const play_years_t *t, int *years, int max);

/** One year's totals: time played, starts, and how many different games were started. */
void play_years_totals (const play_years_t *t, int year, uint32_t *seconds, uint32_t *starts, int *games);

/** The year a time falls in (local time, as the cart's clock keeps it), or 0 if `when` isn't known. */
int play_years_of (long when);

#endif /* PLAY_YEARS_H__ */
