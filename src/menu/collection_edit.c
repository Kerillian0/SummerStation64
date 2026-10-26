/**
 * @file collection_edit.c
 * @brief Putting games in the player's own collections, and making,
 *        renaming and deleting them, from any cover tab.
 */

#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "collection_edit.h"
#include "collections.h"
#include "tabs.h"
#include "collections_ui.h"
#include "dump_check.h"
#include "fonts.h"
#include "games_filter.h"
#include "games_ui.h"
#include "letter_wheel.h"
#include "my_collections.h"
#include "sound.h"
#include "ui_components.h"
#include "ui_components/constants.h"

#define ROWS_SHOWN      (9)         /* rows of the list at once; longer lists scroll */
#define ROW_HEIGHT      (26)
#define LIST_WIDTH      (360)
#define NOTE_MS         (2000)

static enum { NONE, PICKING, NAMING, DELETING } showing = NONE;

static char game_path[300];         /* the game being put in collections ("" for a new empty one) */
static int row = 0;                 /* 0 "New collection", then the collections newest first */
static int first_row = 0;
static int target = -1;             /* the collection being renamed or deleted */
static bool renaming = false;
static char note[64] = "";
static uint64_t note_until = 0;

/* The collection on list row `r` (1 up): newest first, as on the Collections tab. */
static int collection_on_row (int r) {
    return my_collections_count() - r;
}

static void show_note (const char *text) {
    snprintf(note, sizeof(note), "%s", text);
    note_until = get_ticks_ms() + NOTE_MS;
}

static void changed (menu_t *menu) {
    collections_ui_free();      /* the cards' covers may belong to another card now */
    games_filter_collections_changed(menu);
}

static void named (menu_t *menu, const char *name) {
    if (!name) {
        showing = game_path[0] ? PICKING : NONE;    /* gave up: back to the list, or out */
        return;
    }
    if (renaming) {
        my_collections_rename(target, name);
        showing = NONE;
    } else {
        int k = my_collections_create(name);
        if (k < 0) {
            show_note("There are as many collections as there can be (32).");
        } else if (game_path[0]) {
            my_collections_set(k, game_path, true);
        }
        showing = game_path[0] ? PICKING : NONE;   /* back to the list, the new one ticked at the top */
        row = (k >= 0) ? 1 : 0;
        first_row = 0;
    }
    changed(menu);
}

void collection_edit_pick (const char *path) {
    snprintf(game_path, sizeof(game_path), "%s", path ? path : "");
    row = (my_collections_count() > 0) ? 1 : 0;
    first_row = 0;
    showing = PICKING;
}

void collection_edit_new (void) {
    game_path[0] = '\0';
    renaming = false;
    showing = NAMING;
    letter_wheel_name_begin("New collection", NULL, named);
}

void collection_edit_rename (int k) {
    game_path[0] = '\0';
    target = k;
    renaming = true;
    showing = NAMING;
    letter_wheel_name_begin("Rename", my_collections_name(k), named);
}

void collection_edit_delete (int k) {
    target = k;
    showing = DELETING;
}

void collection_edit_remove (menu_t *menu, int k, const char *path) {
    my_collections_set(k, path, false);
    changed(menu);
}

bool collection_edit_process (menu_t *menu) {
    switch (showing) {
        case NAMING:
            letter_wheel_name_process(menu);
            return true;

        case DELETING:
            if (menu->actions.enter) {
                my_collections_delete(target);
                tabs_collection_deleted(collections_mine_id(target));  /* its tab goes; later ones keep theirs */
                showing = NONE;
                sound_play_effect(SFX_SETTING);
                changed(menu);
            } else if (menu->actions.back || menu->actions.options) {
                showing = NONE;
                sound_play_effect(SFX_EXIT);
            }
            return true;

        case PICKING: {
            int rows = 1 + my_collections_count();
            if (menu->actions.go_up && row > 0) {
                row--;
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.go_down && row < rows - 1) {
                row++;
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.back) {
                showing = NONE;
                sound_play_effect(SFX_EXIT);
            } else if (menu->actions.enter) {
                if (row == 0) {
                    renaming = false;
                    showing = NAMING;
                    letter_wheel_name_begin("New collection", NULL, named);
                } else {
                    int k = collection_on_row(row);
                    bool in = my_collections_has(k, game_path);
                    if (!my_collections_set(k, game_path, !in)) {
                        show_note("That collection is full (128 games).");
                    }
                    sound_play_effect(SFX_SETTING);
                    changed(menu);
                }
            }
            if (row < first_row) {
                first_row = row;
            } else if (row >= first_row + ROWS_SHOWN) {
                first_row = row - ROWS_SHOWN + 1;
            }
            return true;
        }

        default:
            return false;
    }
}

void collection_edit_draw (void) {
    if (showing == DELETING) {
        ui_components_messagebox_draw("Delete this collection?\n\n%s\n\n^%02XA: Delete^00    ^%02XB: Keep^00\nThe games stay on the card.",
                                      my_collections_name(target), STL_RED, STL_GREEN);
    } else if (showing == PICKING) {
        int rows = 1 + my_collections_count();
        int shown = (rows < ROWS_SHOWN) ? rows : ROWS_SHOWN;
        int height = 40 + (shown * ROW_HEIGHT);
        ui_components_dialog_draw(LIST_WIDTH, height);
        int x0 = DISPLAY_CENTER_X - (LIST_WIDTH / 2);
        int y0 = DISPLAY_CENTER_Y - (height / 2);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH, .height = 26, .align = ALIGN_CENTER, .valign = VALIGN_CENTER, .style_id = STL_GRAY },
            FNT_DEFAULT, x0, y0 + 6, "Add to collection");
        for (int i = 0; i < shown; i++) {
            int r = first_row + i;
            int y = y0 + 34 + (i * ROW_HEIGHT);
            if (r == row) {
                ui_components_box_draw(x0 + 8, y, x0 + LIST_WIDTH - 8, y + ROW_HEIGHT, CONTEXT_MENU_HIGHLIGHT_COLOR);
            }
            const char *text = "+ New collection";
            if (r > 0) {
                int k = collection_on_row(r);
                text = my_collections_name(k);
                if (my_collections_has(k, game_path)) {
                    games_ui_dump_mark_draw(x0 + 18, y + 3, DUMP_VERIFIED);    /* the green check: the game is in it (the font has no tick) */
                }
            }
            rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH - 80, .height = ROW_HEIGHT, .valign = VALIGN_CENTER, .wrap = WRAP_ELLIPSES },
                FNT_DEFAULT, x0 + 48, y, "%s", text);
        }
        /* More rows than shown: a mark at the end that has more. */
        if (first_row > 0) {
            rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT, x0 + LIST_WIDTH - 28, y0 + 34, "▲");
        }
        if (first_row + shown < rows) {
            rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT, x0 + LIST_WIDTH - 28, y0 + 34 + ((shown - 1) * ROW_HEIGHT), "▼");
        }
    }
    if (note[0] && get_ticks_ms() < note_until) {
        ui_components_messagebox_draw("%s", note);
    }
}
