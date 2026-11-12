/**
 * @file game_status_list.h
 * @brief The status marks themselves (game_status.h shows and keeps them):
 * which game has which status, by its name from the top of the card, and
 * how a line of the status file reads and is written. No console code, so
 * it is tested on the PC (tests/game_status_test.c).
 *
 * The file: "status 1", then one game per line, a letter and the name:
 * "P N64(US)/Game.z64" (P Playing, B Beaten, C 100%, L Backlog).
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

/** A file was renamed: its status follows (unless the new name has one already). @return true if anything changed. */
bool game_status_list_rename (game_status_list_t *list, const char *old_name, const char *new_name);

/** How many games have this status. */
int game_status_list_count (const game_status_list_t *list, game_status_t status);

/** Empty the list (frees the names). */
void game_status_list_clear (game_status_list_t *list);

#endif /* GAME_STATUS_LIST_H__ */
