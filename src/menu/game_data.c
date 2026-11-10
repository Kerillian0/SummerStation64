/**
 * @file game_data.c
 * @brief Genre, most players and how they play together, per game code.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "game_data.h"
#include "menu_features.h"
#include "review_scores.h"

#define DATA_PATH       "rom:/gamedata.txt"
#define DATA_HEADER     "gamedata 1"
#define MAX_CODES       (1024)
#define MAX_GENRES      (40)
#define GENRE_LENGTH    (20)

typedef struct {
    char code[4];
    uint8_t players;
    char mode;
    uint8_t genre;          /* 1 = the first name kept, 0 = none */
} entry_t;

static entry_t entries[MAX_CODES];
static int count = 0;
static char genres[MAX_GENRES][GENRE_LENGTH];
static int genre_count = 0;
static bool loaded = false;

static uint8_t genre_number (const char *name) {
    if (!name[0]) {
        return 0;
    }
    for (int i = 0; i < genre_count; i++) {
        if (strcmp(genres[i], name) == 0) {
            return (uint8_t) (i + 1);
        }
    }
    if (genre_count >= MAX_GENRES) {
        return 0;
    }
    snprintf(genres[genre_count], GENRE_LENGTH, "%s", name);
    return (uint8_t) (++genre_count);
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;
    FILE *f = fopen(DATA_PATH, "r");
    if (!f) {
        debugf("game data: %s missing\n", DATA_PATH);
        return;
    }
    char line[96];
    if (fgets(line, sizeof(line), f) && strncmp(line, DATA_HEADER, strlen(DATA_HEADER)) == 0) {
        while (count < MAX_CODES && fgets(line, sizeof(line), f)) {
            char code[8], mode[4], genre[GENRE_LENGTH + 8] = "";
            int players;
            if (line[0] == '#' || sscanf(line, "%7s %d %3s %27[^\r\n]", code, &players, mode, genre) < 3 || strlen(code) != 4) {
                continue;
            }
            entry_t *e = &entries[count++];
            memcpy(e->code, code, 4);
            e->players = (uint8_t) players;
            e->mode = mode[0];
            e->genre = genre_number(genre);
        }
    }
    fclose(f);
    debugf("game data: %d games, %d genres\n", count, genre_count);
}

const game_data_t *game_data_find (const char *game_code) {
    static game_data_t answer;
    if (!game_code) {
        return NULL;
    }
    load();
    for (int i = 0; i < count; i++) {
        if (memcmp(entries[i].code, game_code, 4) == 0) {
            answer.players = entries[i].players;
            answer.mode = entries[i].mode;
            answer.genre = entries[i].genre ? genres[entries[i].genre - 1] : "";
            return &answer;
        }
    }
    return NULL;
}

const char *game_data_players_text (const game_data_t *data, bool with_word) {
    static char text[40];
    if (!data || data->players <= 0) {
        return "";
    }
    const char *mode = (data->mode == 'C') ? ", co-op" : (data->mode == 'V') ? ", versus" : (data->mode == 'B') ? ", co-op/vs" : "";
    if (data->players == 1) {
        snprintf(text, sizeof(text), with_word ? "1 player" : "1");
    } else {
        snprintf(text, sizeof(text), "1-%d%s%s", data->players, with_word ? " players" : "", mode);
    }
    return text;
}

/* Review scores (review_scores.h), read the first time one is asked for:
   the player's own menu/reviewscores.txt on the SD card. No list comes with
   the menu; without the file, no scores show. */
#define SCORES_SD_PATH  "sd:/menu/reviewscores.txt"

static void review_scores_load (void) {
    static bool tried = false;
    if (tried) {
        return;
    }
    tried = true;
    const char *paths[] = { SCORES_SD_PATH };
    for (int i = 0; i < 1; i++) {
        FILE *f = fopen(paths[i], "r");
        if (!f) {
            continue;
        }
        bool ok = review_scores_read(f);
        fclose(f);
        if (ok) {
            debugf("game data: %d review scores from %s\n", review_scores_count(), paths[i]);
            return;
        }
        debugf("game data: %s is not a scores file\n", paths[i]);
    }
}

int game_data_review_score (const char *game_code) {
    if (!game_code) {
        return 0;
    }
    review_scores_load();
    return review_scores_find(game_code);
}

bool game_data_review_scores_shown (void) {
    if (!features_enabled(FEATURE_REVIEW_SCORES)) {
        return false;
    }
    review_scores_load();
    return review_scores_count() > 0;
}
