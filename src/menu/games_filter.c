/**
 * @file games_filter.c
 * @brief Shows only one region's games, or only 64DD disks, on the Games tab.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libdragon.h>

#include "game_index.h"
#include "games_filter.h"
#include "path.h"
#include "sort_order.h"

typedef enum {
    FILTER_ALL,
    FILTER_USA,
    FILTER_JAPAN,
    FILTER_EUROPE,
    FILTER_64DD,
    FILTER_COUNT
} filter_t;

static const char *names[FILTER_COUNT] = { "All", "USA", "Japan", "Europe", "64DD" };

/* Which filters a game belongs to, one bit each (bit FILTER_ALL always set). */
static uint8_t *marks = NULL;   /* indexed by entry->index, the order the folder was read in */
static int marks_count = 0;
static int total = 0;           /* entries in the list, shown and hidden */
static filter_t current = FILTER_ALL;
static int (*stock_compare) (const void *, const void *) = NULL;   /* the browser's own sort, for the Type order */

static uint8_t from_code_letter (char c) {
    switch (toupper((unsigned char) c)) {
        case 'E': case 'N': return 1 << FILTER_USA;              /* USA, Canada */
        case 'J': return 1 << FILTER_JAPAN;
        case 'P': case 'D': case 'F': case 'I': case 'S': case 'U':
        case 'X': case 'Y': case 'H': case 'W': case 'L':
            return 1 << FILTER_EUROPE;                          /* PAL countries */
        case 'A': return (1 << FILTER_USA) | (1 << FILTER_JAPAN) | (1 << FILTER_EUROPE);
        default: return 0;
    }
}

/* Tags in brackets: "(U)", "(J)", "(E)", "(JU)", "(USA)", "(Japan)", "(Europe)", "(PAL)". */
static uint8_t from_file_name (const char *name) {
    uint8_t found = 0;
    const char *open = name;
    while ((open = strchr(open, '(')) != NULL) {
        const char *close = strchr(open, ')');
        if (!close) {
            break;
        }
        int length = (int) (close - open - 1);
        char tag[24];
        if (length > 0 && length < (int) sizeof(tag)) {
            memcpy(tag, open + 1, length);
            tag[length] = '\0';
            if (!strcasecmp(tag, "USA") || !strcasecmp(tag, "US")) found |= 1 << FILTER_USA;
            else if (!strcasecmp(tag, "Japan") || !strcasecmp(tag, "JP")) found |= 1 << FILTER_JAPAN;
            else if (!strcasecmp(tag, "Europe") || !strcasecmp(tag, "EU") || !strcasecmp(tag, "PAL")
                  || !strcasecmp(tag, "Germany") || !strcasecmp(tag, "France") || !strcasecmp(tag, "Spain")
                  || !strcasecmp(tag, "Italy") || !strcasecmp(tag, "UK") || !strcasecmp(tag, "Australia")) found |= 1 << FILTER_EUROPE;
            else if (length <= 3) {
                /* GoodN64 style: one letter per region, "JU" for both. */
                bool letters = true;
                for (int i = 0; i < length; i++) letters = letters && isupper((unsigned char) tag[i]);
                for (int i = 0; letters && i < length; i++) {
                    char c = tag[i];
                    if (c == 'U') found |= 1 << FILTER_USA;
                    else if (c == 'J') found |= 1 << FILTER_JAPAN;
                    else if (c == 'E' || c == 'G' || c == 'F' || c == 'S' || c == 'I' || c == 'A') found |= 1 << FILTER_EUROPE;
                }
            }
        }
        open = close + 1;
    }
    return found;
}

static uint8_t classify (menu_t *menu, entry_t *e) {
    uint8_t m = 1 << FILTER_ALL;
    if (e->type == ENTRY_TYPE_DISK) {
        return m | (1 << FILTER_64DD);
    }
    if (e->type != ENTRY_TYPE_ROM) {
        return m;
    }
    path_t *path = path_clone_push(menu->browser.directory, e->name);
    const game_index_entry_t *known = game_index_find(path_get(path), e->size);
    path_free(path);
    uint8_t region = known ? from_code_letter(known->code[3]) : 0;
    if (!region) {
        region = from_file_name(e->name);
    }
    return m | region;
}

static bool matches (const entry_t *e, filter_t f) {
    return e->index >= 0 && e->index < marks_count && (marks[e->index] & (1 << f));
}

static int count_for (menu_t *menu, filter_t f) {
    int n = 0;
    for (int i = 0; i < total; i++) {
        n += matches(&menu->browser.list[i], f) ? 1 : 0;
    }
    return n;
}

/* Put the games that match first and show only those (sorted, if asked). */
static void apply (menu_t *menu, bool sort) {
    entry_t *list = menu->browser.list;
    int shown = 0;
    for (int i = 0; i < total; i++) {
        if (matches(&list[i], current)) {
            entry_t keep = list[i];
            list[i] = list[shown];
            list[shown] = keep;
            shown++;
        }
    }
    menu->browser.entries = shown;
    if (sort && stock_compare) {
        sort_order_apply(menu, stock_compare);
    }
}

void games_filter_forget (void) {
    free(marks);
    marks = NULL;
    marks_count = 0;
    total = 0;
}

void games_filter_after_load (menu_t *menu, int (*compare) (const void *, const void *)) {
    stock_compare = compare;
    games_filter_forget();
    total = menu->browser.entries;
    if (total <= 0) {
        return;
    }
    marks = malloc(total);
    if (!marks) {
        total = 0;
        return;
    }
    marks_count = total;
    for (int i = 0; i < total; i++) {
        entry_t *e = &menu->browser.list[i];
        e->index = i;
        marks[i] = classify(menu, e);
    }
    if (current != FILTER_ALL && count_for(menu, current) == 0) {
        current = FILTER_ALL;   /* nothing of that kind any more */
    }
    if (current != FILTER_ALL) {
        apply(menu, false); /* the caller sorts next */
    }
}

bool games_filter_step (menu_t *menu, int direction) {
    if (total <= 0) {
        return false;
    }
    filter_t next = current;
    for (int tries = 0; tries < FILTER_COUNT; tries++) {
        next = (filter_t) ((next + direction + FILTER_COUNT) % FILTER_COUNT);
        if (next == FILTER_ALL || count_for(menu, next) > 0) {
            break;
        }
    }
    if (next == current) {
        return false;
    }

    const char *was = menu->browser.entry ? menu->browser.entry->name : NULL;
    current = next;
    menu->browser.entries = total;
    apply(menu, true);

    menu->browser.selected = (menu->browser.entries > 0) ? 0 : -1;
    for (int i = 0; was && i < menu->browser.entries; i++) {
        if (menu->browser.list[i].name == was) {
            menu->browser.selected = i;
            break;
        }
    }
    menu->browser.entry = (menu->browser.selected >= 0) ? &menu->browser.list[menu->browser.selected] : NULL;
    debugf("games filter: %s, %d games\n", names[current], (int) menu->browser.entries);
    return true;
}

int games_filter_all_entries (int shown) {
    return (total > shown) ? total : shown;
}

const char *games_filter_label (void) {
    return (current == FILTER_ALL) ? NULL : names[current];
}
