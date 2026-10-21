/**
 * @file game_data.h
 * @brief Genre, most players and how they play together, per game code.
 *
 * A table built into the menu file (rom:/gamedata.txt, made by
 * assets/data/make_game_data.py from libretro-database, CC-BY-SA 4.0, and
 * the project's own play_modes.txt). Read once, about 6 KB.
 */

#ifndef GAME_DATA_H__
#define GAME_DATA_H__

typedef struct {
    int players;            /**< most players at once, 0 if not known */
    char mode;              /**< '-' none, 'C' co-op, 'V' versus, 'B' both */
    const char *genre;      /**< e.g. "Racing", or "" if not known */
} game_data_t;

/** What is known about the game with this four-letter code, or NULL. */
const game_data_t *game_data_find (const char *game_code);

/**
 * The players in words, e.g. "1-4 players, versus" (both: "co-op/vs", to fit the Players box) (`with_word` adds
 * "players"); "" if the count isn't known.
 */
const char *game_data_players_text (const game_data_t *data, bool with_word);

#endif /* GAME_DATA_H__ */
