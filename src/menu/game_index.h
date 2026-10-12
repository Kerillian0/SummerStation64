/**
 * @file game_index.h
 * @brief A remembered list of which file is which game.
 *
 * To show a cover the menu has to know a game's four-letter code, and for
 * the badges a few facts about it. Both used to be read from the game's
 * file on the SD card every time, which costs a slow frame. Here they are
 * noted the first time a game is seen and saved to the card
 * (sd:/menu/gameindex.txt), so every later visit, also after a restart,
 * needs no reading at all.
 *
 * A file is recognised by its full path and its size. Rename or replace a
 * game and it is simply looked at afresh. Deleting the saved file is
 * always safe: the list fills up again as games are browsed.
 */

#ifndef GAME_INDEX_H__
#define GAME_INDEX_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char code[4];           /**< the game's code from its header, e.g. NZSE */
    bool has_facts;         /**< the four below are known */
    int8_t players;         /**< number of players, 0 if not known */
    bool needs_expansion;
    bool likes_expansion;
    bool saves;
    bool has_details;       /**< the game's details were looked for (the date may still be unknown) */
    uint16_t date;          /**< release date, 0 if not known; a later date is a larger number */
} game_index_entry_t;

/** Year, month and day of a release date from the list (0 if the date is not known). */
#define GAME_INDEX_DATE(year, month, day)   ((uint16_t) ((((year) - 1980) << 9) | ((month) << 5) | (day)))
#define GAME_INDEX_DATE_YEAR(date)          ((date) ? 1980 + ((date) >> 9) : 0)
#define GAME_INDEX_DATE_MONTH(date)         (((date) >> 5) & 15)
#define GAME_INDEX_DATE_DAY(date)           ((date) & 31)

/**
 * What is remembered about a file.
 * @param size The file's size if known, 0 if not (then only the path is compared).
 * @return NULL if the file has not been seen (or has changed size).
 */
const game_index_entry_t *game_index_find (const char *path, int64_t size);

#define GAME_INDEX_TITLE_LENGTH (20)

/**
 * Read a ROM file's header from the card (15-20 ms) for its four-letter
 * code and its title (`title` has room for GAME_INDEX_TITLE_LENGTH + 1).
 * Nothing is noted: call game_index_set_code() for that.
 * @return false if the file could not be read.
 */
bool game_index_read_header (const char *path, char *code, char *title);

/** Note a file's game code. */
void game_index_set_code (const char *path, int64_t size, const char *code);

/** Note a file's game code and the facts for its badges. */
void game_index_set_facts (const char *path, int64_t size, const char *code, int players, bool needs_expansion, bool likes_expansion, bool saves);

/**
 * Read a game's release date from its metadata pack entry on the card
 * (one small file, 20 ms or so). Nothing is noted.
 * @param metadata_ini Full path of the entry's metadata.ini.
 * @return The date, or 0 if the file is missing or gives none.
 */
uint16_t game_index_read_date (const char *metadata_ini);

/** Note a game's details. A date of 0 means "looked, none found". */
void game_index_set_details (const char *path, int64_t size, uint16_t date);

/** Save the list to the card if anything was added since the last save. */
void game_index_flush (void);

/** True if something was added since the last save. */
bool game_index_changed (void);

#endif /* GAME_INDEX_H__ */
