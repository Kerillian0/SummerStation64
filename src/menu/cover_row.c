/**
 * @file cover_row.c
 * @brief A row of covers for a saved list of games (the Recent and Favorites tabs).
 */

#include <libdragon.h>

#include "carousel.h"
#include "carousel_art.h"
#include "controls.h"
#include "cover_list.h"
#include "cover_row.h"
#include "fonts.h"
#include "game_facts.h"
#include "menu_features.h"
#include "games_ui.h"
#include "sound.h"
#include "ui_components/constants.h"

#define ROW_MAX     (16)

static bookkeeping_item_t *row_items = NULL;
static entry_t row_entries[ROW_MAX];
static int row_places[ROW_MAX];     /* each entry's place in the saved list */
static cover_list_t row;

/* Where each list's row was left (Recent and Favorites each have their
   own), so coming back from a game's info screen or from another tab returns
   to the same cover. Kept while the console is on. */
#define LISTS_REMEMBERED    (2)
static struct {
    bookkeeping_item_t *items;  /* which list; NULL = not used yet */
    int selected;               /* the cover it was on */
    int place;                  /* that game's place in the saved list */
} left[LISTS_REMEMBERED];
static int current = 0;         /* which of those the open row is */

static path_t *row_path (menu_t *menu, int index) {
    (void) menu;
    return path_clone(row_items[row_places[index]].primary_path);
}

void cover_row_open (menu_t *menu, bookkeeping_item_t *items, int count) {
    (void) menu;

    bool seen = false;
    current = 0;
    for (int i = 0; i < LISTS_REMEMBERED; i++) {
        if (left[i].items == items) {
            current = i;
            seen = true;
            break;
        }
        if (!left[i].items) {
            current = i;    /* a free place, unless this list turns up further on */
        }
    }
    int before = seen ? left[current].selected : 0;
    int last_place = seen ? left[current].place : -1;

    row_items = items;
    row.list = row_entries;
    row.entries = 0;
    row.selected = 0;
    row.path = row_path;

    for (int i = 0; i < count && row.entries < ROW_MAX; i++) {
        bookkeeping_item_t *item = &items[i];
        if (item->bookkeeping_type == BOOKKEEPING_TYPE_EMPTY || !path_has_value(item->primary_path)) {
            continue;
        }
        entry_t *entry = &row_entries[row.entries];
        entry->name = path_last_get(item->primary_path);
        entry->type = (item->bookkeeping_type == BOOKKEEPING_TYPE_DISK) ? ENTRY_TYPE_DISK : ENTRY_TYPE_ROM;
        entry->size = 0;
        entry->index = i;
        row_places[row.entries] = i;
        row.entries++;
    }

    if (seen) {
        /* A list shown before: back to the same game, or (after one was
           removed) to the one that took its place. */
        row.selected = (before < row.entries) ? before : (row.entries - 1);
        if (row.selected < 0) {
            row.selected = 0;
        }
        for (int i = 0; i < row.entries; i++) {
            if (row_places[i] == last_place) {
                row.selected = i;
            }
        }
    }
    left[current].items = items;
    left[current].selected = row.selected;
    left[current].place = (row.entries > 0) ? row_places[row.selected] : -1;

    cover_list_use(&row);
}

int cover_row_process (menu_t *menu) {
    if (row.entries <= 0) {
        return -1;
    }

    if (controls_consume_flip_request() && carousel_art_flip()) {
        sound_play_effect(SFX_CURSOR);
    }

    /* controls_remap_tabs() has turned left/right into "up" and "down". */
    bool wrap = menu->settings.wrap_file_list_scrolling && row.entries > 1;
    if (menu->actions.go_up && (row.selected > 0 || wrap)) {
        row.selected = (row.selected > 0) ? (row.selected - 1) : (row.entries - 1);
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && (row.selected < (row.entries - 1) || wrap)) {
        row.selected = (row.selected < (row.entries - 1)) ? (row.selected + 1) : 0;
        sound_play_effect(SFX_CURSOR);
    }

    left[current].selected = row.selected;
    left[current].place = row_places[row.selected];
    return left[current].place;
}

void cover_row_draw (menu_t *menu, const char *empty_message, const char *z_action) {
    if (row.entries <= 0) {
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = DISPLAY_WIDTH,
            .height = DISPLAY_HEIGHT - GAMES_UI_TOPBAR_BOTTOM,
            .align = ALIGN_CENTER,
            .valign = VALIGN_CENTER,
            .style_id = STL_GRAY,
        }, FNT_DEFAULT, 0, GAMES_UI_TOPBAR_BOTTOM, "%s", empty_message);
        return;
    }

    carousel_draw(menu);
    games_ui_hints_backdrop_draw();

    const char *tap = "Load";
    const char *hold = NULL;
    if (row_entries[row.selected].type == ENTRY_TYPE_ROM) {
        controls_rom_actions(&tap, &hold);
    }
    int x = games_ui_hint_draw(GAMES_UI_HINTS_X, 0, "A", tap);
    if (hold) {
        games_ui_hint_draw(x, 0, "Hold", hold);
    }
    if (z_action) {
        games_ui_hint_draw(GAMES_UI_HINTS_X, 1, "Z", z_action);
    }
}

void cover_row_after_draw (menu_t *menu) {
    if (row.entries > 0) {
        carousel_art_update(menu, features_enabled(FEATURE_SIDE_COVERS));
    }
}

void cover_row_close (void) {
    carousel_art_reset();
    game_facts_reset();
    cover_list_use(NULL);
    row_items = NULL;
}
