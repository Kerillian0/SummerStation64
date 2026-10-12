/**
 * @file game_learn.c
 * @brief Gets to know the Games tab's games while the player is idle.
 */

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
static uint64_t time_spent_ms = 0;

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

    /* Find the next ROM the list does not know and read it. One per frame. */
    while (next < list_size) {
        entry_t *e = &list[next++];
        if (e->type != ENTRY_TYPE_ROM) {
            continue;
        }
        path_t *path = path_clone_push(menu->browser.directory, e->name);
        if (game_index_find(path_get(path), e->size)) {
            path_free(path);
            continue;
        }
        uint64_t began = get_ticks_ms();
        char code[4];
        char title[GAME_INDEX_TITLE_LENGTH + 1];
        if (game_index_read_header(path_get(path), code, title) && !(code[1] == 'E' && code[2] == 'D')) {
            game_index_set_code(path_get(path), e->size, code);   /* homebrew is found by title, so not noted */
            games_filter_note_code(e, code);
            learned++;
        }
        time_spent_ms += get_ticks_ms() - began;
        path_free(path);
        return;
    }

    busy = false;
    if (learned > 0) {
        debugf("game learn: %d games read in %d ms\n", learned, (int) time_spent_ms);
        /* Saved by the covers' idle save (carousel_art.c), which takes
           turns with the other idle saves. */
    }
}
