#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libdragon.h>

#include "fonts.h"
#include "games_ui.h"
#include "theme.h"
#include "title_font.h"
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
#define PANEL_Y         (272)
#define PANEL_HEIGHT    (66)
#define PANEL_TITLE     (36)    /* height of the title line */
#define PANEL_LINE      (26)    /* height of the line under it */

/* Position bar */
#define POSITION_Y      (346)
#define POSITION_HEIGHT (20)
/* The whole bar (letter, track, count) takes the middle three quarters of
   the screen, clear of the edges a CRT hides. */
#define POSITION_WIDTH  (((VISIBLE_AREA_X1 - VISIBLE_AREA_X0) * 3) / 4)
#define POSITION_X0     (DISPLAY_CENTER_X - (POSITION_WIDTH / 2))
#define POSITION_X1     (DISPLAY_CENTER_X + (POSITION_WIDTH / 2))
#define TRACK_X0        (POSITION_X0 + 24)
#define TRACK_X1        (POSITION_X1 - 116)    /* room for "999 of 999" */
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

#define BADGE_HEIGHT    (20)
#define BADGE_PADDING   (6)
#define BADGE_SPACING   (8)

/* A word in a small box, as wide as its text. Returns where the next one goes. */
static int badge (int x, int y, const char *label, menu_font_style_t style, bool boxed) {
    int nbytes = strlen(label);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .height = BADGE_HEIGHT,
        .valign = VALIGN_CENTER,
        .style_id = style,
    }, FNT_DEFAULT, label, &nbytes);

    int width = (int) (layout->bbox.x1 - layout->bbox.x0) + (BADGE_PADDING * 2);
    if (boxed) {
        fill(x, y, x + width, y + BADGE_HEIGHT, theme_get()->tab_inactive);
    }
    rdpq_paragraph_render(layout, x + BADGE_PADDING, y);
    rdpq_paragraph_free(layout);

    return x + width + BADGE_SPACING;
}

void games_ui_title_panel_draw (const char *title, const char *detail, const game_facts_t *facts) {
    const theme_t *t = theme_get();
    int x0 = GAMES_UI_CONTENT_X0;
    int x1 = GAMES_UI_CONTENT_X1;
    int line2_y = PANEL_Y + 2 + PANEL_TITLE;

    fill(x0, PANEL_Y, x1, PANEL_Y + PANEL_HEIGHT, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));

    /* The big title font when the name fits on one line in it, the body font otherwise. */
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = x1 - x0 - 24,
        .height = PANEL_TITLE,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
    }, title_font_pick(title, x1 - x0 - 24), x0 + 12, PANEL_Y + 2, "%s", title);

    if (!facts) {
        text(x0 + 12, line2_y, x1 - x0 - 24, PANEL_LINE, ALIGN_LEFT, STL_GRAY, detail);
        return;
    }

    int x = x0 + 12 - BADGE_PADDING;
    int y = line2_y + (PANEL_LINE - BADGE_HEIGHT) / 2;

    if (facts->players > 0) {
        char players[24];
        snprintf(players, sizeof(players), (facts->players == 1) ? "1 player" : "%d players", facts->players);
        x = badge(x, y, players, STL_GRAY, false);
    }
    if (facts->needs_expansion) {
        x = badge(x, y, "Needs Expansion Pak", STL_ORANGE, true);
    } else if (facts->likes_expansion) {
        x = badge(x, y, "Expansion Pak", STL_GREEN, true);
    }
    if (facts->save_found) {
        x = badge(x, y, "Save found", STL_GREEN, true);
    }
    if (facts->favorite) {
        x = badge(x, y, "Favorite", STL_YELLOW, true);
    }

    /* A game with nothing to flag still says what it is. */
    if (x == x0 + 12 - BADGE_PADDING) {
        text(x0 + 12, line2_y, x1 - x0 - 24, PANEL_LINE, ALIGN_LEFT, STL_GRAY, detail);
    }
}

/* Button hints */
#define HINTS_Y         (388)
#define HINT_ROW_HEIGHT (BADGE_HEIGHT + 4)
#define HINT_GAP        (14)

/* The A and B buttons keep their well-known colors; the rest are plain. */
static menu_font_style_t button_style (const char *button) {
    if (strcmp(button, "A") == 0 || strcmp(button, "Hold") == 0) {
        return STL_BLUE;
    }
    if (strcmp(button, "B") == 0) {
        return STL_GREEN;
    }
    return STL_DEFAULT;
}

int games_ui_hint_width (const char *button, const char *action) {
    /* Laid out off-screen is not possible, so measure by building the two
       paragraphs; they are small and freed straight away. */
    int width = 0;
    const char *parts[2] = { button, action };
    for (int i = 0; i < 2; i++) {
        int nbytes = strlen(parts[i]);
        rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { .height = BADGE_HEIGHT }, FNT_DEFAULT, parts[i], &nbytes);
        width += (int) (layout->bbox.x1 - layout->bbox.x0);
        rdpq_paragraph_free(layout);
    }
    return width + (BADGE_PADDING * 2) + BADGE_PADDING;
}

int games_ui_hint_draw (int x, int row, const char *button, const char *action) {
    int y = HINTS_Y + (row * HINT_ROW_HEIGHT);
    x = badge(x, y, button, button_style(button), true) - BADGE_SPACING + BADGE_PADDING;

    int nbytes = strlen(action);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .height = BADGE_HEIGHT,
        .valign = VALIGN_CENTER,
    }, FNT_DEFAULT, action, &nbytes);
    int width = (int) (layout->bbox.x1 - layout->bbox.x0);
    rdpq_paragraph_render(layout, x, y);
    rdpq_paragraph_free(layout);

    return x + width + HINT_GAP;
}

void games_ui_hint_right_draw (int row, const char *button, const char *action) {
    games_ui_hint_draw(GAMES_UI_CONTENT_X1 - games_ui_hint_width(button, action), row, button, action);
}

void games_ui_position_draw (const char *title, int selected, int count) {
    const theme_t *t = theme_get();

    if (count <= 0) {
        return;
    }

    /* The letter you are at, for finding your way through a long list. */
    char letter[2] = { (title && title[0]) ? (char) toupper((unsigned char) title[0]) : ' ', '\0' };
    text(POSITION_X0, POSITION_Y, 20, POSITION_HEIGHT, ALIGN_LEFT, STL_DEFAULT, letter);

    int track_y = POSITION_Y + (POSITION_HEIGHT / 2) - 2;
    fill(TRACK_X0, track_y, TRACK_X1, track_y + 4, t->tab_inactive);

    int travel = TRACK_X1 - TRACK_X0 - MARKER_SIZE;
    int marker_x = TRACK_X0 + ((count > 1) ? (travel * selected) / (count - 1) : 0);
    int marker_y = POSITION_Y + (POSITION_HEIGHT - MARKER_SIZE) / 2;
    fill(marker_x, marker_y, marker_x + MARKER_SIZE, marker_y + MARKER_SIZE, t->accent);

    char position[24];
    snprintf(position, sizeof(position), "%d of %d", selected + 1, count);
    text(TRACK_X1 + 8, POSITION_Y, POSITION_X1 - TRACK_X1 - 8, POSITION_HEIGHT, ALIGN_RIGHT, STL_GRAY, position);
}
