#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "cover_pick.h"
#include "game_index.h"
#include "games_filter.h"
#include "region_pick.h"
#include "path.h"

#define MAX_COPIES  (8)

/* The cover the list was opened on: its copies' names (to find the cover
   again after the list is read anew), its game's code and the regions. */
static char names[MAX_COPIES][256];
static int name_count = 0;
static char code[5];
static char regions[3];         /* 'U', 'E', 'J' as the game has them, in that order */
static int region_count = 0;
static char shown = 0;          /* the region of the copy giving the cover now */
static void (*after_pick) (menu_t *menu) = NULL;

static char row_text[32];
static char texts[4][28];
static int current = 0;
static char note[64];

static void choose (menu_t *menu, void *arg) {
    char region = (char) (intptr_t) arg;
    region_pick_set(code, region);
    if (region) {
        snprintf(note, sizeof(note), "This game's cover is now its %s copy.", region_pick_region_name(region));
    } else {
        snprintf(note, sizeof(note), "This game's cover is chosen as usual again.");
    }
    if (after_pick) {
        after_pick(menu);
    }
}

static int current_row (menu_t *menu) {
    (void) menu;
    return current;
}

static component_context_menu_t pick_menu = {
    .get_default_selection = current_row,
    .list = {
        { .text = texts[0], .action = choose, .arg = (void *) 0 },
        { .text = texts[1], .action = choose },
        { .text = texts[2], .action = choose },
        { .text = texts[3], .action = choose },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

bool cover_pick_offer (menu_t *menu, entry_t *cover, void (*after) (menu_t *menu)) {
    after_pick = after;
    name_count = 0;
    region_count = 0;
    shown = 0;
    code[0] = '\0';
    if (!cover || cover->type != ENTRY_TYPE_ROM) {
        return false;
    }
    entry_t *copies[MAX_COPIES];
    int n = games_filter_region_list(menu, cover, copies, MAX_COPIES);
    if (n < 2) {
        return false;
    }
    bool has[3] = { false, false, false };
    static const char order[3] = { 'U', 'E', 'J' };
    for (int i = 0; i < n; i++) {
        snprintf(names[name_count++], sizeof(names[0]), "%s", copies[i]->name);
        path_t *path = path_clone_push(menu->browser.directory, copies[i]->name);
        const game_index_entry_t *known = game_index_find(path_get(path), copies[i]->size);
        path_free(path);
        if (!known) {
            continue;
        }
        if (!code[0]) {
            memcpy(code, known->code, 4);
            code[4] = '\0';
        }
        char r = region_pick_region(known->code);
        if (copies[i] == cover) {
            shown = r;
        }
        for (int k = 0; k < 3; k++) {
            has[k] |= (r == order[k]);
        }
    }
    for (int k = 0; k < 3; k++) {
        if (has[k]) {
            regions[region_count++] = order[k];
        }
    }
    if (region_count < 2 || !code[0]) {
        return false;
    }

    char hand = region_pick_by_hand(code);
    current = 0;
    for (int k = 0; k < region_count; k++) {
        if (regions[k] == hand) {
            current = k + 1;
        }
    }
    /* The present choice marked with the arrow the font has; the others lined up with it. */
    snprintf(texts[0], sizeof(texts[0]), "%s Automatic", (current == 0) ? "▶" : "  ");
    for (int k = 0; k < region_count; k++) {
        snprintf(texts[k + 1], sizeof(texts[0]), "%s %s", (current == k + 1) ? "▶" : "  ", region_pick_region_name(regions[k]));
        pick_menu.list[k + 1].text = texts[k + 1];
        pick_menu.list[k + 1].arg = (void *) (intptr_t) regions[k];
    }
    pick_menu.list[region_count + 1].text = NULL;   /* the list ends at the first row without text */
    snprintf(row_text, sizeof(row_text), "Cover: %s", region_pick_region_name(shown));
    return true;
}

const char *cover_pick_row_text (void) {
    return row_text;
}

component_context_menu_t *cover_pick_menu (void) {
    return &pick_menu;
}

void cover_pick_select (menu_t *menu) {
    for (int i = 0; i < menu->browser.entries; i++) {
        for (int k = 0; k < name_count; k++) {
            if (strcmp(menu->browser.list[i].name, names[k]) == 0) {
                menu->browser.selected = i;
                menu->browser.entry = &menu->browser.list[i];
                return;
            }
        }
    }
}

const char *cover_pick_note (void) {
    return note;
}
