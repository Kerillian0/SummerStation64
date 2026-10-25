/**
 * @file collections_ui.c
 * @brief The Collections tab's row of cards, one per collection.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "baked_art.h"
#include "collections.h"
#include "collections_ui.h"
#include "fonts.h"
#include "game_index.h"
#include "games_filter.h"
#include "games_ui.h"
#include "path.h"
#include "sound.h"
#include "theme.h"
#include "ui_components/constants.h"

#define FAN_COVERS      (3)
#define SETTLE_MS       (150)       /* the selection rests this long before its covers load */
#define MIN_FREE        (320 * 1024)    /* a cover is loaded only while this much is free (side covers keep 288-384) */
#define BACK_TAP_MS     (300)       /* B let go sooner than this is a tap */

#define CARD_Y          (180)       /* the middle of the row, as the cover row's */
#define FAN_W           (150)
#define FAN_H           (106)
#define FAN_SPREAD      (72)        /* how far the outer covers sit from the middle one */
#define FAN_TURN        (0.24f)     /* and how much they lean (radians, about 14 degrees) */
#define SIDE_X          (226)       /* the cards either side: their middles' distance from the screen's */
#define SIDE_W          (112)
#define SIDE_H          (96)

static int selected = 0;
static uint64_t moved_at = 0;

/* The selected card's covers, and the card they belong to. */
static int fan_card = -1;
static int fan_count = 0;           /* games in the collection */
static char fan_codes[FAN_COVERS][5];
static component_boxart_t *fan_art[FAN_COVERS];
static bool fan_tried[FAN_COVERS];

static bool b_down = false;
static uint64_t b_down_at = 0;

int collections_ui_selected (void) {
    return collections_id(selected);
}

void collections_ui_free (void) {
    for (int i = 0; i < FAN_COVERS; i++) {
        baked_art_release(fan_art[i]);
        fan_art[i] = NULL;
        fan_tried[i] = false;
    }
    fan_card = -1;
}

/* Which games the selected card shows, and how many it has. */
static void fan_choose (menu_t *menu) {
    collections_ui_free();
    fan_card = selected;
    entry_t *first[FAN_COVERS] = { NULL };
    fan_count = games_filter_collection_covers(menu, collections_id(selected), first, FAN_COVERS);
    for (int i = 0; i < FAN_COVERS; i++) {
        fan_codes[i][0] = '\0';
        if (i < fan_count && first[i]) {
            path_t *path = path_clone_push(menu->browser.directory, first[i]->name);
            const game_index_entry_t *known = game_index_find(path_get(path), first[i]->size);
            path_free(path);
            if (known) {
                memcpy(fan_codes[i], known->code, 4);
                fan_codes[i][4] = '\0';
            }
        }
    }
}

collections_ui_action_t collections_ui_process (menu_t *menu) {
    int count = collections_count();
    if (selected >= count) {
        selected = (count > 0) ? count - 1 : 0;
    }

    joypad_buttons_t pressed = {0};
    JOYPAD_PORT_FOREACH (i) {
        pressed = joypad_get_buttons_pressed(i);
        if (pressed.raw) {
            break;
        }
    }
    if (pressed.l) {
        return COLLECTIONS_UI_TAB_LEFT;
    }
    if (pressed.r) {
        return COLLECTIONS_UI_TAB_RIGHT;
    }
    if (menu->actions.settings) {
        return COLLECTIONS_UI_SETTINGS;
    }
    if (count <= 0) {
        return COLLECTIONS_UI_NONE;
    }
    int step = (menu->actions.go_right || menu->actions.go_down) ? 1 : ((menu->actions.go_left || menu->actions.go_up) ? -1 : 0);
    if (step) {
        int next = selected + step;
        if (menu->settings.wrap_file_list_scrolling) {
            next = (next + count) % count;
        }
        if (next >= 0 && next < count && next != selected) {
            selected = next;
            moved_at = get_ticks_ms();
            sound_play_effect(SFX_CURSOR);
        }
    }
    if (menu->actions.enter) {
        sound_play_effect(SFX_ENTER);
        return COLLECTIONS_UI_OPEN;
    }
    return COLLECTIONS_UI_NONE;
}

void collections_ui_after_draw (menu_t *menu) {
    if (collections_count() <= 0) {
        return;
    }
    if (fan_card != selected) {
        if ((get_ticks_ms() - moved_at) < SETTLE_MS) {
            return;
        }
        fan_choose(menu);
    }
    /* One cover a frame. */
    for (int i = 0; i < FAN_COVERS; i++) {
        if (fan_tried[i] || !fan_codes[i][0]) {
            continue;
        }
        fan_tried[i] = true;
        heap_stats_t heap;
        sys_get_heap_stats(&heap);
        if ((heap.total - heap.used) >= MIN_FREE) {
            fan_art[i] = baked_art_open(menu->storage_prefix, fan_codes[i], "", IMAGE_BOXART_FRONT);
        }
        return;
    }
}

bool collections_ui_back_tap (void) {
    bool down = false;
    JOYPAD_PORT_FOREACH (i) {
        if (joypad_get_buttons(i).b) {     /* the state now: "held" is false on the frame B goes down */
            down = true;
            break;
        }
    }
    if (down && !b_down) {
        b_down = true;
        b_down_at = get_ticks_ms();
    } else if (!down && b_down) {
        b_down = false;
        return (get_ticks_ms() - b_down_at) < BACK_TAP_MS;
    }
    return false;
}

static void box (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_set_mode_fill(color);
    rdpq_fill_rectangle(x0, y0, x1, y1);
}

static void name_text (int x0, int y0, int width, int height, menu_font_style_t style, const char *text) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width,
        .height = height,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_WORD,
        .style_id = style,
    }, FNT_DEFAULT, x0, y0, "%s", text);
}

/* One cover of the fan, or a plain box while it has no picture. */
static void fan_cover (int i, int x, int y, float turn) {
    const theme_t *t = theme_get();
    component_boxart_t *art = fan_art[i];
    if (art && !art->loading && art->image) {
        surface_t *image = art->image;
        float scale = fminf((float) FAN_W / image->width, (float) FAN_H / image->height);
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(image, x, y, &(rdpq_blitparms_t) {
            .cx = image->width / 2,
            .cy = image->height / 2,
            .scale_x = scale,
            .scale_y = scale,
            .theta = -turn,
        });
        return;
    }
    if (turn == 0.0f) {
        box(x - FAN_W / 2, y - FAN_H / 2, x + FAN_W / 2, y + FAN_H / 2, RGBA32(t->tab_inactive.r, t->tab_inactive.g, t->tab_inactive.b, 0xFF));
    } else {
        /* The outer ones peek out from behind: a box beside the middle. */
        int side = (turn < 0) ? -1 : 1;
        int cx = x + side * 8;
        box(cx - FAN_W / 2, y - FAN_H / 2 + 6, cx + FAN_W / 2, y + FAN_H / 2 - 6, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xFF));
    }
}

void collections_ui_draw (menu_t *menu) {
    const theme_t *t = theme_get();
    int count = collections_count();
    if (count <= 0) {
        name_text(GAMES_UI_CONTENT_X0, 100, DISPLAY_WIDTH - (2 * GAMES_UI_CONTENT_X0), 200, STL_GRAY,
            "No collections yet.\n\n"
            "Collections are series (The Legend of Zelda, Mario Party, ...) and multiplayer games. "
            "The menu finds them as it learns your games, while the Games tab sits idle.");
        games_ui_hints_backdrop_draw();
        games_ui_hint_right_draw(0, "START", "Settings");
        return;
    }

    /* The cards either side: their names. */
    for (int side = -1; side <= 1; side += 2) {
        int card = selected + side;
        if (menu->settings.wrap_file_list_scrolling && count > 2) {
            card = (card + count) % count;
        }
        if (card < 0 || card >= count) {
            continue;
        }
        int cx = DISPLAY_CENTER_X + side * SIDE_X;
        box(cx - SIDE_W / 2, CARD_Y - SIDE_H / 2, cx + SIDE_W / 2, CARD_Y + SIDE_H / 2, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xFF));
        name_text(cx - SIDE_W / 2 + 6, CARD_Y - SIDE_H / 2, SIDE_W - 12, SIDE_H, STL_GRAY, collections_name(collections_id(card)));
    }

    /* The selected card: its first covers fanned out, the first on top. */
    if (fan_card == selected) {
        if (fan_count >= 3) {
            fan_cover(2, DISPLAY_CENTER_X + FAN_SPREAD, CARD_Y + 6, FAN_TURN);
        }
        if (fan_count >= 2) {
            fan_cover(1, DISPLAY_CENTER_X - FAN_SPREAD, CARD_Y + 6, -FAN_TURN);
        }
        fan_cover(0, DISPLAY_CENTER_X, CARD_Y, 0.0f);
    } else {
        box(DISPLAY_CENTER_X - FAN_W / 2, CARD_Y - FAN_H / 2, DISPLAY_CENTER_X + FAN_W / 2, CARD_Y + FAN_H / 2,
            RGBA32(t->tab_inactive.r, t->tab_inactive.g, t->tab_inactive.b, 0xFF));
    }

    char games[24];
    int n = (fan_card == selected) ? fan_count : games_filter_collection_covers(menu, collections_id(selected), NULL, 0);
    snprintf(games, sizeof(games), (n == 1) ? "1 game" : "%d games", n);
    const char *name = collections_name(collections_id(selected));
    games_ui_title_panel_draw(name, NULL, games, false, false, NULL);
    games_ui_position_draw(name, selected, count, NULL);

    games_ui_hints_backdrop_draw();
    games_ui_hint_draw(GAMES_UI_HINTS_X, 0, "A", "Open");
    games_ui_hint_right_draw(0, "START", "Settings");
}
