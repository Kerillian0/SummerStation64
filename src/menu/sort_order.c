#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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
        case SORT_DEVELOPER: return "Developer";
        case SORT_RECENTLY_ADDED: return "Recently Added";
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
static path_t *detail_directory = NULL;

/* What the game list knows about an entry. The developer's name stays
   valid afterwards; the rest only until the next question. */
static const game_index_entry_t *known_game (path_t *directory, const entry_t *e) {
    if (e->type != ENTRY_TYPE_ROM || !directory) {
        return NULL;
    }
    path_t *path = path_clone_push(directory, (char *) e->name);
    const game_index_entry_t *known = game_index_find(path_get(path), e->size);
    path_free(path);
    return known;
}

static unsigned int release_date (const entry_t *e) {
    const game_index_entry_t *known = known_game(detail_directory, e);
    return (known && known->date) ? known->date : 0xFFFF;
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

static const char *developer (const entry_t *e) {
    const game_index_entry_t *known = known_game(detail_directory, e);
    return known ? known->developer : NULL;
}

static int compare_developer (const void *pa, const void *pb) {
    int order = folders_first(pa, pb);
    if (order) {
        return order;
    }
    const char *a = developer(pa);
    const char *b = developer(pb);
    if (!a || !b) {
        if (a || b) {
            return a ? -1 : 1;      /* games with no known developer go last */
        }
        return compare_name_az(pa, pb);
    }
    order = strcasecmp(a, b);
    return order ? order : compare_release(pa, pb);
}

/* Days since a game was first seen (game_index_added_days()), or a large
   number for games that count as old, which go last. */
static int added_days (path_t *directory, const entry_t *e) {
    if (e->type != ENTRY_TYPE_ROM || !directory) {
        return 0x7FFFFFFF;
    }
    path_t *path = path_clone_push(directory, (char *) e->name);
    int days = game_index_added_days(path_get(path), e->size);
    path_free(path);
    return (days < 0) ? 0x7FFFFFFF : days;
}

static int compare_added (const void *pa, const void *pb) {
    int order = folders_first(pa, pb);
    if (order) {
        return order;
    }
    int a = added_days(detail_directory, pa);
    int b = added_days(detail_directory, pb);
    return (a != b) ? ((a < b) ? -1 : 1) : compare_name_az(pa, pb);
}

const char *sort_order_detail (path_t *directory, const entry_t *entry) {
    static char text[24];
    int order = options_get(OPTION_SORT_ORDER);
    if (order == SORT_RECENTLY_ADDED) {
        int days = added_days(directory, entry);
        if (days == 0x7FFFFFFF) {
            return NULL;
        }
        if (days < 2) {
            return days ? "Added yesterday" : "Added today";
        }
        if (days < 60) {
            snprintf(text, sizeof(text), "Added %d days ago", days);
            return text;
        }
        /* Longer ago: the day itself. */
        static const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        time_t when = time(NULL) - (time_t) days * 86400;
        struct tm *tm = gmtime(&when);
        snprintf(text, sizeof(text), "Added %s %d, %d", months[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        return text;
    }
    if (order != SORT_RELEASE_DATE && order != SORT_DEVELOPER) {
        return NULL;
    }
    const game_index_entry_t *known = known_game(directory, entry);
    if (!known) {
        return NULL;
    }
    if (order == SORT_DEVELOPER) {
        return known->developer;
    }
    if (!known->date) {
        return NULL;
    }
    snprintf(text, sizeof(text), "Released %d", (int) GAME_INDEX_DATE_YEAR(known->date));
    return text;
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
            detail_directory = menu->browser.directory;
            compare = compare_release;
            break;
        case SORT_DEVELOPER:
            detail_directory = menu->browser.directory;
            compare = compare_developer;
            break;
        case SORT_RECENTLY_ADDED:
            detail_directory = menu->browser.directory;
            compare = compare_added;
            break;
        default:
            break;
    }

    qsort(menu->browser.list, menu->browser.entries, sizeof(entry_t), compare);
}
