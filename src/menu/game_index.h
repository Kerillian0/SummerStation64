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
} game_index_entry_t;

/**
 * What is remembered about a file.
 * @param size The file's size if known, 0 if not (then only the path is compared).
 * @return NULL if the file has not been seen (or has changed size).
 */
const game_index_entry_t *game_index_find (const char *path, int64_t size);

/** Note a file's game code. */
void game_index_set_code (const char *path, int64_t size, const char *code);

/** Note a file's game code and the facts for its badges. */
void game_index_set_facts (const char *path, int64_t size, const char *code, int players, bool needs_expansion, bool likes_expansion, bool saves);

/** Save the list to the card if anything was added since the last save. */
void game_index_flush (void);

#endif /* GAME_INDEX_H__ */
