#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "menu_options.h"
#include "path.h"
#include "sort_order.h"
#include "games_folders.h"
#include "game_index.h"

#define NAME_LENGTH (256)

/* Names of the recently played games that live in the folder being sorted,
   most recent first. Filled in before sorting; qsort can't pass them along. */
static char recent[HISTORY_COUNT][NAME_LENGTH];
static int recent_count = 0;

const char *sort_order_name (int order) {
    switch (order) {
        case SORT_NAME_AZ: return "Name A-Z";
        case SORT_NAME_ZA: return "Name Z-A";
        case SORT_RECENT_FIRST: return "Recently Played";
        case SORT_RELEASE_DATE: return "Release Date";
        default: return "Type";
    }
}

static int folders_first (const entry_t *a, const entry_t *b) {
    bool a_dir = (a->type == ENTRY_TYPE_DIR);
    bool b_dir = (b->type == ENTRY_TYPE_DIR);
    return (a_dir == b_dir) ? 0 : (a_dir ? -1 : 1);
}

static int compare_name_az (const void *pa, const void *pb) {
    const entry_t *a = pa;
    const entry_t *b = pb;
    int order = folders_first(a, b);
    return order ? order : strcasecmp(entry_file_name(a->name), entry_file_name(b->name));
}

static int compare_name_za (const void *pa, const void *pb) {
    const entry_t *a = pa;
    const entry_t *b = pb;
    int order = folders_first(a, b);
    return order ? order : strcasecmp(entry_file_name(b->name), entry_file_name(a->name));
}

/* 0 for the most recently played game, counting up; recent_count for everything else. */
static int recent_rank (const entry_t *e) {
    for (int i = 0; i < recent_count; i++) {
        if (strcmp(e->name, recent[i]) == 0) {
            return i;
        }
    }
    return recent_count;
}

static int compare_recent (const void *pa, const void *pb) {
    int a = recent_rank(pa);
    int b = recent_rank(pb);
    return (a != b) ? (a - b) : compare_name_az(pa, pb);
}

/* Release dates come from the remembered game list, which is filled in
   while the Games tab sits idle. A game not looked up yet counts as
   unknown and goes after the dated ones. */
static menu_t *dated_menu = NULL;

static unsigned int release_date (const entry_t *e) {
    if (e->type != ENTRY_TYPE_ROM || !dated_menu) {
        return 0xFFFF;
    }
    path_t *path = path_clone_push(dated_menu->browser.directory, (char *) e->name);
    const game_index_entry_t *known = game_index_find(path_get(path), e->size);
    unsigned int date = (known && known->date) ? known->date : 0xFFFF;
    path_free(path);
    return date;
}

static int compare_release (const void *pa, const void *pb) {
    int order = folders_first(pa, pb);
    if (order) {
        return order;
    }
    unsigned int a = release_date(pa);
    unsigned int b = release_date(pb);
    return (a != b) ? ((a < b) ? -1 : 1) : compare_name_az(pa, pb);
}

static void find_recent (menu_t *menu) {
    recent_count = 0;

    for (int i = 0; i < HISTORY_COUNT; i++) {
        bookkeeping_item_t *item = &menu->bookkeeping.history_items[i];
        if (item->bookkeeping_type == BOOKKEEPING_TYPE_EMPTY || !item->primary_path) {
            continue;
        }

        /* Is this game in the list being shown? Its name in the list is its
           path from the open folder (on the Games tab that is the top of the
           card, so the name carries its folder). */
        const char *full = path_get(item->primary_path);
        const char *dir = path_get(menu->browser.directory);
        size_t dir_length = strlen(dir);
        while (dir_length > 0 && dir[dir_length - 1] == '/') dir_length--;
        bool here = (strncmp(full, dir, dir_length) == 0) && (full[dir_length] == '/');
        const char *name = here ? full + dir_length + 1 : NULL;
        if (here && (path_is_root(menu->browser.directory) || !strchr(name, '/'))) { /* not from a folder further down */
            strncpy(recent[recent_count], name, NAME_LENGTH - 1);
            recent[recent_count][NAME_LENGTH - 1] = '\0';
            recent_count++;
        }
    }
}

void sort_order_apply (menu_t *menu, int (*stock_compare) (const void *, const void *)) {
    int (*compare) (const void *, const void *) = stock_compare;

    switch (options_get(OPTION_SORT_ORDER)) {
        case SORT_NAME_AZ:
            compare = compare_name_az;
            break;
        case SORT_NAME_ZA:
            compare = compare_name_za;
            break;
        case SORT_RECENT_FIRST:
            find_recent(menu);
            compare = compare_recent;
            break;
        case SORT_RELEASE_DATE:
            dated_menu = menu;
            compare = compare_release;
            break;
        default:
            break;
    }

    qsort(menu->browser.list, menu->browser.entries, sizeof(entry_t), compare);
}
