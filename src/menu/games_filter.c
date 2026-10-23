/**
 * @file games_filter.c
 * @brief Shows only one region's games, or only 64DD disks, on the Games tab,
 *        and only homebrew on the Homebrew tab.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libdragon.h>

#include "game_index.h"
#include "games_filter.h"
#include "games_folders.h"
#include "path.h"
#include "menu_options.h"
#include "rom_hacks.h"
#include "sort_order.h"

typedef enum {
    FILTER_ALL,
    FILTER_USA,
    FILTER_JAPAN,
    FILTER_EUROPE,
    FILTER_64DD,
    FILTER_HACKS,
    FILTER_STEPS,               /* C-up / C-down step through the ones above */
    FILTER_HOMEBREW = FILTER_STEPS,     /* the Homebrew tab's shelf */
    FILTER_COUNT
} filter_t;

static const char *names[FILTER_COUNT] = { "All", "USA", "Japan", "Europe", "64DD", "Hacks", "Homebrew" };

/* Which filters a game belongs to, one bit each (bit FILTER_ALL always set). */
static uint8_t *marks = NULL;   /* indexed by entry->index, the order the folder was read in */
static int marks_count = 0;
static int total = 0;           /* entries in the list, shown and hidden */

/* Game Versions: Grouped. Files sharing a game code (an original, its hacks
   and copies of it) show as one cover; the others are folded under it. */
#define MARK_FOLDED     (1 << 7)    /* in marks: folded under another file's cover */
static int16_t *group = NULL;       /* by entry->index: the index of its cover's file, or -1 */

const char *games_filter_versions_name (int choice) {
    static const char *versions_names[GAME_VERSIONS_COUNT] = { "Separate", "Grouped" };
    return (choice >= 0 && choice < GAME_VERSIONS_COUNT) ? versions_names[choice] : "";
}

/* Whether folded files are left out for this filter: with Grouped on, except
   on the Hacks step, which shows every hack. */
static bool folding (filter_t f) {
    return f != FILTER_HACKS && options_get(OPTION_GAME_VERSIONS) == GAME_VERSIONS_GROUPED;
}
static filter_t current = FILTER_ALL;
static filter_t stepped = FILTER_ALL;   /* the Games tab's step, kept while the shelf shows */
static bool shelf = false;              /* the Homebrew tab is showing */
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
    if (known && game_index_is_homebrew(known->code)) {
        return m | (1 << FILTER_HOMEBREW) | from_file_name(e->name);  /* its code's last letter says nothing */
    }
    uint8_t region = known ? from_code_letter(known->code[3]) : 0;
    if (!region) {
        region = from_file_name(e->name);
    }
    return m | region;
}

static bool matches (const entry_t *e, filter_t f) {
    return e->index >= 0 && e->index < marks_count && (marks[e->index] & (1 << f))
        && !((marks[e->index] & MARK_FOLDED) && folding(f));
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

/* Which games are hacks of others (rom_hacks.h): told from the file names
   and from games sharing a code in the remembered game list. The working
   lists are given back at once. */
static void mark_hacks (menu_t *menu) {
    const char **names = malloc(total * sizeof(char *));
    char (*codes)[4] = calloc(total, 4);
    bool *is_rom = malloc(total * sizeof(bool));
    bool *hack = malloc(total * sizeof(bool));
    long long *sizes = malloc(total * sizeof(long long));
    if (names && codes && is_rom && hack && sizes) {
        for (int i = 0; i < total; i++) {
            entry_t *e = &menu->browser.list[i];
            names[i] = entry_file_name(e->name);
            is_rom[i] = (e->type == ENTRY_TYPE_ROM);
            sizes[i] = e->size;
            if (is_rom[i]) {
                path_t *path = path_clone_push(menu->browser.directory, e->name);
                const game_index_entry_t *known = game_index_find(path_get(path), e->size);
                if (known && !game_index_is_homebrew(known->code)) {
                    memcpy(codes[i], known->code, 4);   /* unrelated homebrew shares one code: no hacks or versions by it */
                }
                path_free(path);
            }
        }
        rom_hacks_find(names, codes, is_rom, total, hack);
        /* Every game in a folder the player marked as holding hacks. */
        int in_folders = 0;
        for (int i = 0; i < total; i++) {
            if (is_rom[i] && games_folders_entry_in_hacks(menu->browser.list[i].name)) {
                in_folders += hack[i] ? 0 : 1;
                hack[i] = true;
            }
        }
        if (in_folders > 0) {
            debugf("games filter: %d more hacks from folders marked as hacks\n", in_folders);
        }
        int found = 0;
        for (int i = 0; i < total; i++) {
            if (hack[i]) {
                marks[menu->browser.list[i].index] |= 1 << FILTER_HACKS;
                found++;
            }
        }
        /* Versions: the files of each game code, under one cover. The cover
           is the original's (a file that isn't a hack, a "[!]" good dump
           first), or the first file if all of them are hacks. */
        if (group) {
            for (int i = 0; i < total; i++) {
                group[menu->browser.list[i].index] = -1;
            }
            for (int i = 0; i < total; i++) {
                if (!is_rom[i] || memcmp(codes[i], "\0\0\0\0", 4) == 0 || group[menu->browser.list[i].index] >= 0) {
                    continue;
                }
                int cover = i, best = -1;
                for (int j = i; j < total; j++) {
                    if (is_rom[j] && memcmp(codes[i], codes[j], 4) == 0) {
                        int score = (hack[j] ? 0 : 2) + (strstr(names[j], "[!]") ? 1 : 0);
                        if (score > best) {
                            best = score;
                            cover = j;
                        }
                    }
                }
                for (int j = i; j < total; j++) {
                    if (is_rom[j] && memcmp(codes[i], codes[j], 4) == 0) {
                        group[menu->browser.list[j].index] = menu->browser.list[cover].index;
                        if (j != cover) {
                            marks[menu->browser.list[j].index] |= MARK_FOLDED;
                        }
                    }
                }
            }
        }
        /* For checking the rules: every game code more than one file shares,
           with each file's size and what it was taken for. */
        for (int i = 0; i < total; i++) {
            bool first = is_rom[i] && memcmp(codes[i], "\0\0\0\0", 4) != 0;
            bool shared = false;
            for (int j = 0; first && j < total; j++) {
                if (j != i && is_rom[j] && memcmp(codes[i], codes[j], 4) == 0) {
                    if (j < i) first = false;
                    shared = true;
                }
            }
            if (!first || !shared) {
                continue;
            }
            for (int j = i; j < total; j++) {
                if (is_rom[j] && memcmp(codes[i], codes[j], 4) == 0) {
                    debugf("games filter: code %.4s: %s (%d KB)%s\n", codes[i], names[j], (int) (sizes[j] / 1024), hack[j] ? " - hack" : "");
                }
            }
        }
        for (int i = 0; i < total; i++) {
            if (hack[i] && rom_hacks_name_says(names[i])) {
                debugf("games filter: %s - hack, by its name\n", names[i]);
            }
        }
        debugf("games filter: %d hacks among %d games\n", found, total);
    }
    free(names);
    free(codes);
    free(is_rom);
    free(hack);
    free(sizes);
}

/* Show the current set again, staying on the game that was selected when
   it is still in the set. */
static void reapply (menu_t *menu) {
    const char *was = menu->browser.entry ? menu->browser.entry->name : NULL;
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
}

void games_filter_recheck_hacks (menu_t *menu) {
    if (total <= 0 || !marks) {
        return;
    }
    for (int i = 0; i < marks_count; i++) {
        marks[i] &= ~((1 << FILTER_HACKS) | MARK_FOLDED);
    }
    mark_hacks(menu);   /* the whole list, hidden games included */
    debugf("games filter: %d homebrew games known\n", count_for(menu, FILTER_HOMEBREW));
    if (current == FILTER_HACKS || current == FILTER_HOMEBREW) {
        reapply(menu);          /* the Hacks step or the shelf is showing: show the new set */
    }
    /* With Game Versions grouped, newly learned codes fold more files: done
       at the next load, so the row doesn't change under the player. */
}

bool games_filter_is_hack (const entry_t *e) {
    return matches(e, FILTER_HACKS);
}

bool games_filter_is_hack_path (menu_t *menu, const char *path) {
    if (!marks || !path || strncmp(path, "sd:/", 4) != 0 || !menu->browser.list) {
        return false;
    }
    int all = games_filter_all_entries(menu->browser.entries);
    for (int i = 0; i < all; i++) {
        if (strcmp(menu->browser.list[i].name, path + 4) == 0) {
            return games_filter_is_hack(&menu->browser.list[i]);
        }
    }
    return false;
}

void games_filter_forget (void) {
    free(marks);
    marks = NULL;
    free(group);
    group = NULL;
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
    group = malloc(total * sizeof(int16_t));   /* without it, versions simply aren't grouped */
    marks_count = total;
    for (int i = 0; i < total; i++) {
        entry_t *e = &menu->browser.list[i];
        e->index = i;
        marks[i] = classify(menu, e);
    }
    mark_hacks(menu);
    debugf("games filter: %d homebrew games known\n", count_for(menu, FILTER_HOMEBREW));
    if (!shelf && current != FILTER_ALL && count_for(menu, current) == 0) {
        current = FILTER_ALL;   /* nothing of that kind any more */
    }
    if (current != FILTER_ALL || folding(current)) {
        apply(menu, false); /* the caller sorts next */
    }
}

bool games_filter_step (menu_t *menu, int direction) {
    if (total <= 0 || shelf) {
        return false;
    }
    filter_t next = current;
    for (int tries = 0; tries < FILTER_STEPS; tries++) {
        next = (filter_t) ((next + direction + FILTER_STEPS) % FILTER_STEPS);
        if (next == FILTER_ALL || count_for(menu, next) > 0) {
            break;
        }
    }
    if (next == current) {
        return false;
    }

    current = next;
    reapply(menu);
    debugf("games filter: %s, %d games\n", names[current], (int) menu->browser.entries);
    return true;
}

void games_filter_note_code (const entry_t *e, const char *code) {
    if (e->index < 0 || e->index >= marks_count) {
        return;
    }
    if (game_index_is_homebrew(code)) {
        marks[e->index] |= 1 << FILTER_HOMEBREW;    /* on the shelf once the reader finishes (games_filter_recheck_hacks) */
        return;
    }
    uint8_t region_bits = (1 << FILTER_USA) | (1 << FILTER_JAPAN) | (1 << FILTER_EUROPE);
    if (!(marks[e->index] & region_bits)) {
        marks[e->index] |= from_code_letter(code[3]);
    }
}

int games_filter_all_entries (int shown) {
    return (total > shown) ? total : shown;
}

const char *games_filter_label (void) {
    return (current == FILTER_ALL || shelf) ? NULL : names[current];
}

void games_filter_shelf (menu_t *menu, bool homebrew, bool list_loaded) {
    if (homebrew == shelf) {
        return;
    }
    shelf = homebrew;
    if (homebrew) {
        stepped = current;
        current = FILTER_HOMEBREW;
    } else {
        current = stepped;
    }
    if (list_loaded && total > 0) {
        reapply(menu);
    }
}

int games_filter_versions (menu_t *menu, const entry_t *e) {
    if (!group || !e || e->index < 0 || e->index >= marks_count || !folding(current)) {
        return 1;
    }
    int n = 0;
    int all = games_filter_all_entries(menu->browser.entries);
    for (int i = 0; i < all; i++) {
        int k = menu->browser.list[i].index;
        n += (k >= 0 && k < marks_count && group[k] == e->index) ? 1 : 0;
    }
    return (n > 0) ? n : 1;
}

int games_filter_version_list (menu_t *menu, const entry_t *e, entry_t **out, int most) {
    int n = 0;
    if (!group || !e || e->index < 0 || e->index >= marks_count) {
        return 0;
    }
    int all = games_filter_all_entries(menu->browser.entries);
    for (int i = 0; i < all && n < most; i++) {
        int k = menu->browser.list[i].index;
        if (k >= 0 && k < marks_count && group[k] == e->index) {
            out[n++] = &menu->browser.list[i];
        }
    }
    return n;
}
