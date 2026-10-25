/**
 * @file collections.c
 * @brief Groups of games for the Collections tab, worked out by the menu.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "collections.h"
#include "game_data.h"
#include "game_index.h"
#include "games_filter.h"
#include "path.h"

#define SERIES_PATH     "rom:/series.txt"
#define SERIES_HEADER   "series 1"
#define SERIES_MAX      (24)
#define SERIES_CODES    (320)
#define NAME_LENGTH     (32)

/* The built-in series: each code the number of its series. */
static char series_names[SERIES_MAX][NAME_LENGTH];
static int series_count = 0;
static struct {
    char code[4];
    uint8_t series;
} series_codes[SERIES_CODES];
static int series_code_count = 0;
static bool series_loaded = false;

/* The multiplayer ones, after the series. */
enum { MULTI_TWO, MULTI_FOUR, MULTI_COOP, MULTI_VERSUS, MULTI_COUNT };
static const char *const multi_names[MULTI_COUNT] = { "2+ Players", "4 Players", "Co-op", "Versus" };

/* Every collection that could be offered, by number: the series, then the
   multiplayer ones. `offered` lists those with games, in order. */
#define ALL_COLLECTIONS (SERIES_MAX + MULTI_COUNT)
static int offered[COLLECTIONS_MAX];
static int offered_count = 0;

static uint32_t *member = NULL;     /* by entry->index: one bit per collection, by its number */
static int member_count = 0;

static void load_series (void) {
    if (series_loaded) {
        return;
    }
    series_loaded = true;
    FILE *f = fopen(SERIES_PATH, "r");
    if (!f) {
        debugf("collections: %s missing\n", SERIES_PATH);
        return;
    }
    char line[96];
    if (fgets(line, sizeof(line), f) && strncmp(line, SERIES_HEADER, strlen(SERIES_HEADER)) == 0) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '#' || !line[0]) {
                continue;
            }
            if (line[0] == '=' && line[1] == ' ') {
                if (series_count < SERIES_MAX) {
                    snprintf(series_names[series_count++], NAME_LENGTH, "%s", line + 2);
                }
                continue;
            }
            /* A line of codes for the series named last. */
            for (char *code = strtok(line, " "); code && series_count > 0; code = strtok(NULL, " ")) {
                if (strlen(code) == 4 && series_code_count < SERIES_CODES) {
                    memcpy(series_codes[series_code_count].code, code, 4);
                    series_codes[series_code_count].series = (uint8_t) (series_count - 1);
                    series_code_count++;
                }
            }
        }
    }
    fclose(f);
    debugf("collections: %d series, %d game codes\n", series_count, series_code_count);
}

static int series_of (const char *code) {
    for (int i = 0; i < series_code_count; i++) {
        if (memcmp(series_codes[i].code, code, 4) == 0) {
            return series_codes[i].series;
        }
    }
    return -1;
}

/* Which of every possible collection (series number, or SERIES_MAX + the
   multiplayer one) a game with this code is in, as bits of a 64-bit mask. */
static uint64_t collections_of (const char *code, int indexed_players) {
    uint64_t in = 0;
    int series = series_of(code);
    if (series >= 0) {
        in |= 1ull << series;
    }
    const game_data_t *data = game_data_find(code);
    int players = (data && data->players) ? data->players : indexed_players;
    char mode = data ? data->mode : '-';
    if (players >= 2) in |= 1ull << (SERIES_MAX + MULTI_TWO);
    if (players >= 4) in |= 1ull << (SERIES_MAX + MULTI_FOUR);
    if (mode == 'C' || mode == 'B') in |= 1ull << (SERIES_MAX + MULTI_COOP);
    if (mode == 'V' || mode == 'B') in |= 1ull << (SERIES_MAX + MULTI_VERSUS);
    return in;
}

void collections_forget (void) {
    free(member);
    member = NULL;
    member_count = 0;
    offered_count = 0;
}

void collections_build (menu_t *menu) {
    load_series();
    collections_forget();
    int all = games_filter_all_entries(menu->browser.entries);
    if (all <= 0) {
        return;
    }
    uint64_t *in = calloc(all, sizeof(uint64_t));
    member = calloc(all, sizeof(uint32_t));
    if (!in || !member) {
        free(in);
        collections_forget();
        return;
    }
    member_count = all;

    uint64_t used = 0;
    for (int i = 0; i < all; i++) {
        entry_t *e = &menu->browser.list[i];
        if (e->type != ENTRY_TYPE_ROM || e->index < 0 || e->index >= all) {
            continue;
        }
        path_t *path = path_clone_push(menu->browser.directory, e->name);
        const game_index_entry_t *known = game_index_find(path_get(path), e->size);
        path_free(path);
        if (known && !game_index_is_homebrew(known->code)) {
            in[e->index] = collections_of(known->code, known->players);
            used |= in[e->index];
        }
    }

    /* Offer the ones with games: the series in the list's order, then the multiplayer ones. */
    for (int c = 0; c < ALL_COLLECTIONS && offered_count < COLLECTIONS_MAX; c++) {
        if ((c < series_count || c >= SERIES_MAX) && (used & (1ull << c))) {
            offered[offered_count++] = c;
        }
    }
    for (int i = 0; i < all; i++) {
        member[i] = (uint32_t) in[i];
    }
    free(in);
    debugf("collections: %d with games\n", offered_count);
}

int collections_count (void) {
    return offered_count;
}

int collections_id (int place) {
    return (place >= 0 && place < offered_count) ? offered[place] : -1;
}

const char *collections_name (int id) {
    if (id >= 0 && id < series_count) {
        return series_names[id];
    }
    if (id >= SERIES_MAX && id < ALL_COLLECTIONS) {
        return multi_names[id - SERIES_MAX];
    }
    return "";
}

bool collections_has (int id, const entry_t *e) {
    return member && e && id >= 0 && id < ALL_COLLECTIONS && e->index >= 0 && e->index < member_count
        && (member[e->index] & (1u << id));
}
