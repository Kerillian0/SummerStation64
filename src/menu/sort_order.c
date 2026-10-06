#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "menu_options.h"
#include "path.h"
#include "sort_order.h"

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
    return order ? order : strcasecmp(a->name, b->name);
}

static int compare_name_za (const void *pa, const void *pb) {
    const entry_t *a = pa;
    const entry_t *b = pb;
    int order = folders_first(a, b);
    return order ? order : strcasecmp(b->name, a->name);
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

static void find_recent (menu_t *menu) {
    recent_count = 0;

    for (int i = 0; i < HISTORY_COUNT; i++) {
        bookkeeping_item_t *item = &menu->bookkeeping.history_items[i];
        if (item->bookkeeping_type == BOOKKEEPING_TYPE_EMPTY || !item->primary_path) {
            continue;
        }

        /* Is this game in the folder being shown? */
        path_t *folder = path_clone(item->primary_path);
        path_pop(folder);
        bool here = (strcmp(path_get(folder), path_get(menu->browser.directory)) == 0);
        path_free(folder);

        if (here) {
            strncpy(recent[recent_count], path_last_get(item->primary_path), NAME_LENGTH - 1);
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
        default:
            break;
    }

    qsort(menu->browser.list, menu->browser.entries, sizeof(entry_t), compare);
}
