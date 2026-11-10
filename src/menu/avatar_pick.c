#include <stdio.h>

#include "avatar_pick.h"
#include "avatars.h"
#include "fonts.h"
#include "games_ui.h"
#include "profiles.h"
#include "sound.h"
#include "theme.h"
#include "ui_components.h"
#include "ui_components/constants.h"

#define COLS            (5)
#define ROWS            (3)
#define PICTURE         (64)
#define STEP            (88)
#define HIGHLIGHT       (10)    /* the accent box round the chosen cell reaches this far past the picture */
#define TITLE_Y         (VISIBLE_AREA_Y0 + 10)
#define TITLE_HEIGHT    (26)
#define GRID_X          (DISPLAY_CENTER_X - ((COLS * STEP) / 2) + ((STEP - PICTURE) / 2))
#define GRID_Y          (TITLE_Y + TITLE_HEIGHT + 28)
#define LABEL_Y         (GRID_Y + (ROWS * STEP) - 8)

static bool open = false;
static int profile;
static int selected;
static int top_row;

void avatar_pick_open (int which) {
    avatars_rescan();
    profile = which;
    selected = avatars_find(profiles_picture(profile));
    top_row = 0;
    open = true;
}

bool avatar_pick_active (void) {
    return open;
}

static void close_grid (void) {
    open = false;
    avatars_forget_unused();    /* keep only the players' own pictures */
}

void avatar_pick_process (menu_t *menu) {
    int count = avatars_count();
    int was = selected;
    if (menu->actions.go_left && selected > 0) {
        selected--;
    } else if (menu->actions.go_right && selected < count - 1) {
        selected++;
    } else if (menu->actions.go_up && selected >= COLS) {
        selected -= COLS;
    } else if (menu->actions.go_down && (selected / COLS) < ((count - 1) / COLS)) {
        selected = (selected + COLS < count) ? selected + COLS : count - 1;
    } else if (menu->actions.enter) {
        profiles_set_picture(profile, avatars_ref(selected));
        debugf("avatars: %s now has \"%s\"\n", profiles_name(profile), avatars_ref(selected));
        sound_play_effect(SFX_SETTING);
        close_grid();
        return;
    } else if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        close_grid();
        return;
    }
    if (selected != was) {
        sound_play_effect(SFX_CURSOR);
    }
    int row = selected / COLS;
    if (row < top_row) {
        top_row = row;
    } else if (row >= top_row + ROWS) {
        top_row = row - ROWS + 1;
    }
}

static void text (int x, int y, int width, rdpq_align_t align, menu_font_style_t style, const char *s) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width,
        .height = TITLE_HEIGHT,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, x, y, "%s", s);
}

void avatar_pick_draw (void) {
    const theme_t *t = theme_get();
    int count = avatars_count();
    char title[64];
    snprintf(title, sizeof(title), "PICTURE FOR %s", profiles_name(profile));
    text(VISIBLE_AREA_X0, TITLE_Y, VISIBLE_AREA_WIDTH, ALIGN_CENTER, STL_GRAY, title);

    color_t color = profiles_color(profile);
    for (int i = top_row * COLS; i < count && i < (top_row + ROWS) * COLS; i++) {
        int x = GRID_X + ((i % COLS) * STEP);
        int y = GRID_Y + (((i / COLS) - top_row) * STEP);
        if (i == selected) {
            games_ui_round_fill(x - HIGHLIGHT, y - HIGHLIGHT, x + PICTURE + HIGHLIGHT, y + PICTURE + HIGHLIGHT, t->accent);
        }
        avatars_draw(avatars_ref(i), x, y, PICTURE, color, profiles_name(profile));
    }

    /* A thin bar at the right when there are more rows than fit. */
    int rows = (count + COLS - 1) / COLS;
    if (rows > ROWS) {
        int x = GRID_X + (COLS * STEP) - ((STEP - PICTURE) / 2) + 8;
        int height = ROWS * STEP - (STEP - PICTURE);
        int thumb = (height * ROWS) / rows;
        int at = GRID_Y + ((height - thumb) * top_row) / (rows - ROWS);
        games_ui_round_fill(x, GRID_Y, x + 4, GRID_Y + height, t->tab_inactive);
        games_ui_round_fill(x, at, x + 4, at + thumb, t->accent);
    }

    const char *label = avatars_label(avatars_ref(selected));
    if (count == 1) {
        label = "No pictures yet: put PNG files in menu/avatars on the SD card.";
    }
    text(VISIBLE_AREA_X0, LABEL_Y, VISIBLE_AREA_WIDTH, ALIGN_CENTER, STL_DEFAULT, label);

    ui_components_actions_bar_text_draw(STL_DEFAULT, ALIGN_LEFT, VALIGN_TOP, "A: Choose\nB: Back");
}
