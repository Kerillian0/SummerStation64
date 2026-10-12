/**
 * @file game_learn.c
 * @brief Gets to know the Games tab's games while the player is idle.
 */

#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "game_index.h"
#include "game_learn.h"
#include "games_filter.h"
#include "path.h"

/* How long the selection must rest before reading starts. Longer than any
   cover or badge wait, so this never competes with them. */
#define REST_MS     (1500)

static entry_t *list = NULL;     /* the list being worked through */
static int list_size = 0;
static int next = 0;             /* the next entry to look at */
static int selected = -1;
static uint64_t rested_since = 0;
static bool busy = false;
static int learned = 0;
static int detailed = 0;          /* games whose details were looked up */
static uint64_t time_spent_ms = 0;

/* Note the details in the metadata pack entry for this game code. */
static void learn_details (menu_t *menu, const char *file, int64_t size, const char *code) {
    char sub[48];
    snprintf(sub, sizeof(sub), "menu/metadata/%c/%c/%c/%c/metadata.ini", code[0], code[1], code[2], code[3]);
    path_t *path = path_init(menu->storage_prefix, sub);
    uint16_t date;
    char developer[GAME_INDEX_DEVELOPER_LENGTH];
    game_index_read_details(path_get(path), &date, developer);
    path_free(path);
    game_index_set_details(file, size, date, developer);
}

bool game_learn_busy (void) {
    return busy;
}

void game_learn_update (menu_t *menu) {
    uint64_t now = get_ticks_ms();
    int all = games_filter_all_entries(menu->browser.entries);   /* games the filter hides too */

    if (menu->browser.list != list || all != list_size) {
        /* Another list (or the same one read again): start from the top. */
        list = menu->browser.list;
        list_size = all;
        next = 0;
        learned = 0;
        detailed = 0;
        time_spent_ms = 0;
        busy = (all > 0);
    }
    if (menu->browser.selected != selected) {
        selected = menu->browser.selected;
        rested_since = now;
    }
    if (!busy || (now - rested_since) < REST_MS) {
        return;
    }

    /* Find the next ROM the list does not know everything about and read
       one thing: its header for the code, or, once that is known, its
       metadata pack entry for the details. One read per frame. */
    while (next < list_size) {
        entry_t *e = &list[next];
        if (e->type != ENTRY_TYPE_ROM) {
            next++;
            continue;
        }
        path_t *path = path_clone_push(menu->browser.directory, e->name);
        const game_index_entry_t *known = game_index_find(path_get(path), e->size);
        if (known && known->has_details) {
            path_free(path);
            next++;
            continue;
        }
        uint64_t began = get_ticks_ms();
        if (!known) {
            char code[4];
            char title[GAME_INDEX_TITLE_LENGTH + 1];
            if (game_index_read_header(path_get(path), code, title) && !(code[1] == 'E' && code[2] == 'D')) {
                game_index_set_code(path_get(path), e->size, code);   /* homebrew is found by title, so not noted */
                games_filter_note_code(e, code);
                learned++;
            }
            if (!game_index_find(path_get(path), e->size)) {
                next++;     /* not a game the list keeps; otherwise its details are read next frame */
            }
        } else {
            char code[4];
            memcpy(code, known->code, 4);
            learn_details(menu, path_get(path), e->size, code);
            detailed++;
            next++;
        }
        time_spent_ms += get_ticks_ms() - began;
        path_free(path);
        return;
    }

    busy = false;
    if (learned > 0 || detailed > 0) {
        debugf("game learn: %d games read, details of %d looked up, in %d ms\n", learned, detailed, (int) time_spent_ms);
        learned = 0;
        detailed = 0;
        /* Saved by the covers' idle save (carousel_art.c), which takes
           turns with the other idle saves. */
    }
}
