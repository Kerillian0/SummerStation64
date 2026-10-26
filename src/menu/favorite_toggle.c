/**
 * @file favorite_toggle.c
 * @brief Favorite or unfavorite a game straight from its cover.
 */

#include <string.h>

#include "bookkeeping.h"
#include "favorite_toggle.h"
#include "game_facts.h"

bool favorite_toggle_allowed (const entry_t *e) {
    return e && (e->type == ENTRY_TYPE_ROM || e->type == ENTRY_TYPE_DISK);
}

static int find (menu_t *menu, const char *path) {
    for (int i = 0; i < FAVORITES_COUNT; i++) {
        bookkeeping_item_t *item = &menu->bookkeeping.favorite_items[i];
        if (item->bookkeeping_type != BOOKKEEPING_TYPE_EMPTY && item->primary_path &&
            strcmp(path_get(item->primary_path), path) == 0) {
            return i;
        }
    }
    return -1;
}

bool favorite_toggle_is (menu_t *menu, const char *path) {
    return find(menu, path) >= 0;
}

const char *favorite_toggle_text (menu_t *menu, const char *path) {
    return favorite_toggle_is(menu, path) ? "Remove from Favorites" : "Add to Favorites";
}

bool favorite_toggle (menu_t *menu, const char *path, bool disk) {
    int at = find(menu, path);
    if (at >= 0) {
        bookkeeping_favorite_remove(&menu->bookkeeping, at);
    } else {
        path_t *p = path_create(path);
        bookkeeping_favorite_add(&menu->bookkeeping, p, NULL, disk ? BOOKKEEPING_TYPE_DISK : BOOKKEEPING_TYPE_ROM);
        path_free(p);
    }
    game_facts_recheck();   /* the heart on the title panel follows */
    return at < 0;
}
