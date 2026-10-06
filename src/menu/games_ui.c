#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include <libdragon.h>

#include "fonts.h"
#include "games_ui.h"
#include "theme.h"
#include "ui_components/constants.h"

/* Tab bar. Kept well inside the picture: a CRT hides its outer edge, and the
   top-left corner most of all. */
#define TOPBAR_INSET    (16)
#define TOPBAR_Y        (34)
#define TOPBAR_HEIGHT   (GAMES_UI_TOPBAR_BOTTOM - TOPBAR_Y)
#define BADGE_WIDTH     (22)
#define TOPBAR_TAB_WIDTH       (104)
#define TOPBAR_TAB_GAP         (6)
#define TABS_X          (VISIBLE_AREA_X0 + TOPBAR_INSET + BADGE_WIDTH + TOPBAR_TAB_GAP + 2)
#define CLOCK_WIDTH     (100)

/* Title panel */
#define PANEL_Y         (280)
#define PANEL_HEIGHT    (56)
#define PANEL_LINE      (26)

/* Position bar */
#define POSITION_Y      (346)
#define POSITION_HEIGHT (20)
#define TRACK_X0        (VISIBLE_AREA_X0 + 28)
#define TRACK_X1        (VISIBLE_AREA_X1 - 88)
#define MARKER_SIZE     (10)

static void fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        if (color.a == 0xFF) {
            rdpq_set_mode_fill(color);
        } else {
            rdpq_set_mode_standard();
            rdpq_set_prim_color(color);
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

static void text (int x, int y, int width, int height, rdpq_align_t align, menu_font_style_t style, const char *string) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width,
        .height = height,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, x, y, "%s", string);
}

/* A small labelled box: a tab, a button badge or the clock. */
static void pill (int x, int width, const char *label, bool active) {
    const theme_t *t = theme_get();

    if (active) {
        /* The selected tab is outlined in the accent color. */
        fill(x - 2, TOPBAR_Y - 2, x + width + 2, TOPBAR_Y + TOPBAR_HEIGHT + 2, t->accent);
        fill(x, TOPBAR_Y, x + width, TOPBAR_Y + TOPBAR_HEIGHT, t->tab_active);
    } else {
        fill(x, TOPBAR_Y, x + width, TOPBAR_Y + TOPBAR_HEIGHT, t->tab_inactive);
    }
    text(x, TOPBAR_Y, width, TOPBAR_HEIGHT, ALIGN_CENTER, active ? STL_DEFAULT : STL_GRAY, label);
}

void games_ui_topbar_draw (menu_t *menu, games_tab_t selected) {
    static const char *const labels[GAMES_TAB_COUNT] = { "Games", "Recent", "Favorites" };

    int x = VISIBLE_AREA_X0 + TOPBAR_INSET;
    pill(x, BADGE_WIDTH, "L", false);

    x = TABS_X;
    for (int i = 0; i < GAMES_TAB_COUNT; i++) {
        pill(x, TOPBAR_TAB_WIDTH, labels[i], i == (int) selected);
        x += TOPBAR_TAB_WIDTH + TOPBAR_TAB_GAP;
    }
    pill(x + 2, BADGE_WIDTH, "R", false);

    /* The clock only shows when the cart's clock is working. */
    if (menu->current_time >= 0) {
        struct tm *now = localtime(&menu->current_time);
        if (now) {
            char clock[16];
            int hour = now->tm_hour % 12;
            snprintf(clock, sizeof(clock), "%d:%02d %s", hour ? hour : 12, now->tm_min, (now->tm_hour < 12) ? "AM" : "PM");
            pill(VISIBLE_AREA_X1 - TOPBAR_INSET - CLOCK_WIDTH, CLOCK_WIDTH, clock, false);
        }
    }
}

void games_ui_title_panel_draw (const char *title, const char *detail) {
    const theme_t *t = theme_get();
    int x0 = VISIBLE_AREA_X0;
    int x1 = VISIBLE_AREA_X1;

    fill(x0, PANEL_Y, x1, PANEL_Y + PANEL_HEIGHT, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));

    text(x0 + 12, PANEL_Y + 3, x1 - x0 - 24, PANEL_LINE, ALIGN_LEFT, STL_DEFAULT, title);
    text(x0 + 12, PANEL_Y + 3 + PANEL_LINE - 2, x1 - x0 - 24, PANEL_LINE, ALIGN_LEFT, STL_GRAY, detail);
}

void games_ui_position_draw (const char *title, int selected, int count) {
    const theme_t *t = theme_get();

    if (count <= 0) {
        return;
    }

    /* The letter you are at, for finding your way through a long list. */
    char letter[2] = { (title && title[0]) ? (char) toupper((unsigned char) title[0]) : ' ', '\0' };
    text(VISIBLE_AREA_X0 + 4, POSITION_Y, 20, POSITION_HEIGHT, ALIGN_LEFT, STL_DEFAULT, letter);

    int track_y = POSITION_Y + (POSITION_HEIGHT / 2) - 2;
    fill(TRACK_X0, track_y, TRACK_X1, track_y + 4, t->tab_inactive);

    int travel = TRACK_X1 - TRACK_X0 - MARKER_SIZE;
    int marker_x = TRACK_X0 + ((count > 1) ? (travel * selected) / (count - 1) : 0);
    int marker_y = POSITION_Y + (POSITION_HEIGHT - MARKER_SIZE) / 2;
    fill(marker_x, marker_y, marker_x + MARKER_SIZE, marker_y + MARKER_SIZE, t->accent);

    char position[24];
    snprintf(position, sizeof(position), "%d of %d", selected + 1, count);
    text(TRACK_X1 + 8, POSITION_Y, VISIBLE_AREA_X1 - TRACK_X1 - 8, POSITION_HEIGHT, ALIGN_RIGHT, STL_GRAY, position);
}
