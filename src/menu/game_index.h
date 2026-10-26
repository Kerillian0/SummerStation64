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

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char code[4];           /**< the game's code from its header, e.g. NZSE */
    bool has_facts;         /**< the five below are known */
    int8_t players;         /**< number of players, 0 if not known */
    bool needs_expansion;
    bool likes_expansion;
    bool saves;
    uint8_t paks;           /**< accessories it uses, GAME_INDEX_PAK_... bits */
    bool has_details;       /**< the game's details were looked for (the date may still be unknown) */
    uint16_t date;          /**< release date, 0 if not known; a later date is a larger number */
    const char *developer;  /**< who made the game, NULL if not known; stays valid while the menu runs */
} game_index_entry_t;

#define GAME_INDEX_DEVELOPER_LENGTH (40)

/** Accessories a game uses, from the built-in game database. */
#define GAME_INDEX_PAK_CONTROLLER   (1 << 0)
#define GAME_INDEX_PAK_RUMBLE       (1 << 1)
#define GAME_INDEX_PAK_TRANSFER     (1 << 2)

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

/**
 * How many days ago this game was first seen (0: today), for Recently
 * Added and the New badge. -1 when it counts as old: noted before the menu
 * kept the day, on the card's first run (the list was new then), or the
 * clock isn't set. A game the list doesn't know yet has only just turned
 * up, so it counts as today.
 */
int game_index_added_days (const char *path, int64_t size);

#define GAME_INDEX_TITLE_LENGTH (20)

/**
 * Read a ROM file's header from the card (15-20 ms) for its four-letter
 * code and its title (`title` has room for GAME_INDEX_TITLE_LENGTH + 1).
 * Nothing is noted: call game_index_set_code() for that.
 * @return false if the file could not be read.
 */
bool game_index_read_header (const char *path, char *code, char *title);

/**
 * Whether a game code is homebrew's: the one homebrew tools write ("ED" in
 * the middle, e.g. NEDE), or, as a fallback, no proper code at all (blanks
 * or symbols where four letters or digits should be). Many unrelated
 * homebrew games share these, and their art is found by title, not by code.
 * The list keeps the second kind as NED0 (game_index_set_code()).
 */
static inline bool game_index_is_homebrew (const char *code) {
    if (code[1] == 'E' && code[2] == 'D') {
        return true;
    }
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) code[i])) {
            return true;
        }
    }
    return false;
}

/**
 * Give homebrew without a proper code the homebrew code's shape ("ED" in the
 * middle), so the stock screens look its art up by its title too. For a
 * game's details after they are loaded (how it runs is settled by then).
 */
static inline void game_index_homebrew_by_title (char *code) {
    if (game_index_is_homebrew(code)) {
        code[1] = 'E';
        code[2] = 'D';
    }
}

/** Note a file's game code. */
void game_index_set_code (const char *path, int64_t size, const char *code);

/** Note a file's game code and the facts for its badges. */
void game_index_set_facts (const char *path, int64_t size, const char *code, int players, bool needs_expansion, bool likes_expansion, bool saves, uint8_t paks);

/**
 * Read a game's details from its metadata pack entry on the card (one
 * small file, 10 ms or so). Nothing is noted.
 * @param metadata_ini Full path of the entry's metadata.ini.
 * @param date Receives the release date, 0 if the file is missing or gives none.
 * @param developer Receives who made the game ("" if not given); room for
 *                  GAME_INDEX_DEVELOPER_LENGTH. The pack writes "Developer |
 *                  Publisher"; the publisher is left out.
 */
void game_index_read_details (const char *metadata_ini, uint16_t *date, char *developer);

/** Note a game's details. A date of 0 or an empty developer means "looked, none found". */
void game_index_set_details (const char *path, int64_t size, uint16_t date, const char *developer);

/** Save the list to the card if anything was added since the last save. */
void game_index_flush (void);

/** True if something was added since the last save. */
bool game_index_changed (void);

/** A game's file was renamed (file_rename.h): its note goes with it, saved at once. */
void game_index_rename (const char *old_path, const char *new_path, int64_t size);

#endif /* GAME_INDEX_H__ */
