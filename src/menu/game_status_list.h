/**
 * @file game_status_list.h
 * @brief The status marks themselves (game_status.h shows and keeps them):
 * which game has which status, by its name from the top of the card, and
 * how a line of the status file reads and is written. No console code, so
 * it is tested on the PC (tests/game_status_test.c).
 *
 * The file: "status 1", then one game per line, a letter and the name:
 * "P N64(US)/Game.z64" (P Playing, B Beaten, C 100%, L Backlog). A game
 * beaten (or 100%) since this was added also has the day it was first marked
 * so, days since 2000-01-01, on a line of its own after it:
 * "D 9447 N64(US)/Game.z64" (older menus skip these lines). Year in Review
 * counts the games beaten each year from it.
 */

#ifndef GAME_STATUS_LIST_H__
#define GAME_STATUS_LIST_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME_STATUS_HEADER  "status 1"

typedef enum {
    GAME_STATUS_NONE,
    GAME_STATUS_PLAYING,
    GAME_STATUS_BEATEN,
    GAME_STATUS_COMPLETE,       /**< 100% */
    GAME_STATUS_BACKLOG,
    GAME_STATUS_COUNT
} game_status_t;

/** Most games with a status. */
#define GAME_STATUS_MAX     (512)

typedef struct {
    uint32_t hash;      /**< of the name, to look up quickly */
    uint8_t status;
    uint16_t day;       /**< Beaten / 100%: the day it was first marked so (days since 2000), 0 if not known */
    char *name;         /**< from the top of the card, allocated */
} game_status_row_t;

typedef struct {
    game_status_row_t rows[GAME_STATUS_MAX];
    int count;
} game_status_list_t;

/** Read a line ("B N64(US)/Game.z64"): the status and the name, copied into `name`. @return false if it isn't one. */
bool game_status_parse (const char *line, game_status_t *status, char *name, size_t name_size);

/** Write a line, with its line end. @return its length (as snprintf). */
int game_status_format (game_status_t status, const char *name, char *out, size_t size);

/** A game's status (GAME_STATUS_NONE if it has none). */
game_status_t game_status_list_get (const game_status_list_t *list, const char *name);

/** Set a game's status; GAME_STATUS_NONE takes it off the list. @return false if the list is full (or memory ran out). */
bool game_status_list_set (game_status_list_t *list, const char *name, game_status_t status);

/**
 * The same, on day `today` (days since 2000): a game becoming Beaten or 100%
 * from anything else is dated today; from Beaten to 100% (or back) it keeps
 * its day; any other status forgets it.
 */
bool game_status_list_set_on (game_status_list_t *list, const char *name, game_status_t status, unsigned today);

/** Read a day line ("D 9447 N64/Game.z64"). @return false if it isn't one. */
bool game_status_parse_day (const char *line, unsigned *day, char *name, size_t name_size);

/** Write a game's day line, with its line end ("" when it has no day). @return its length (as snprintf). */
int game_status_format_day (const game_status_row_t *row, char *out, size_t size);

/** Give a game on the list its day (from a day line). @return false if it isn't on the list as Beaten or 100%. */
bool game_status_list_set_day (game_status_list_t *list, const char *name, unsigned day);

/** The year of a day counted from 2000-01-01 (day 0). */
int game_status_day_year (unsigned day);

/** The games first beaten (or 100%) in this year: how many, and the first `max` rows in `out`. */
int game_status_list_beaten_in (const game_status_list_t *list, int year, const game_status_row_t **out, int max);

/** The years games were beaten in, newest first. @return how many. */
int game_status_list_beaten_years (const game_status_list_t *list, int *years, int max);

/** A file was renamed: its status follows (unless the new name has one already). @return true if anything changed. */
bool game_status_list_rename (game_status_list_t *list, const char *old_name, const char *new_name);

/** How many games have this status. */
int game_status_list_count (const game_status_list_t *list, game_status_t status);

/** Empty the list (frees the names). */
void game_status_list_clear (game_status_list_t *list);

#endif /* GAME_STATUS_LIST_H__ */
