#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libdragon.h>

#include "fonts.h"
#include "games_ui.h"
#include "tabs.h"
#include "menu_features.h"
#include "theme.h"
#include "title_font.h"
#include "ui_components/constants.h"

/* Tab bar. Kept well inside the picture: a CRT hides its outer edge, and the
   top-left corner most of all. */
#define TOPBAR_INSET    (16)
#define TOPBAR_Y        (34)
#define TOPBAR_HEIGHT   (GAMES_UI_TOPBAR_BOTTOM - TOPBAR_Y)
#define TOPBAR_TAB_WIDTH       (104)
#define TOPBAR_TAB_GAP         (6)
#define TABS_X          (VISIBLE_AREA_X0 + TOPBAR_INSET)
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
        /* The selected tab is filled with the accent color, a little larger
           than the others, with whichever of black or white text shows best
           on it. An outline alone was hard to read on a composite TV. */
        int brightness = ((t->accent.r * 3) + (t->accent.g * 6) + t->accent.b) / 10;
        fill(x - 2, TOPBAR_Y - 2, x + width + 2, TOPBAR_Y + TOPBAR_HEIGHT + 2, t->accent);
        text(x, TOPBAR_Y, width, TOPBAR_HEIGHT, ALIGN_CENTER, (brightness >= 128) ? STL_BLACK : STL_WHITE, label);
        return;
    }
    fill(x, TOPBAR_Y, x + width, TOPBAR_Y + TOPBAR_HEIGHT, t->tab_inactive);
    text(x, TOPBAR_Y, width, TOPBAR_HEIGHT, ALIGN_CENTER, STL_GRAY, label);
}

/* Under the clock: what sits in the console's memory slot, and how much
   memory that gives: the Expansion Pak with its red lid, or the plain
   Jumper Pak that consoles shipped with. */
#define PAK_WIDTH       (24)
#define MEMORY_Y        (GAMES_UI_TOPBAR_BOTTOM + 6)
#define MEMORY_TEXT     (62)    /* room for "8MB" with "Detected" under it */

static void memory_badge_draw (void) {
    bool expanded = is_memory_expanded();
    int x1 = VISIBLE_AREA_X1 - TOPBAR_INSET;
    int x0 = x1 - MEMORY_TEXT - PAK_WIDTH;

    /* A small picture of the pak (drawn by assets/images/make_icons.py).
       Only the one this console has is ever loaded. */
    static sprite_t *pak = NULL;
    if (!pak) {
        pak = sprite_load(expanded ? "rom:/expansion_pak.sprite" : "rom:/jumper_pak.sprite");
    }
    x0 = x1 - MEMORY_TEXT - pak->width;
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_alphacompare(1);
        rdpq_sprite_blit(pak, x0, MEMORY_Y, NULL);
    rdpq_mode_pop();
    /* Two lines beside the pak: the amount, and in small green "Detected". */
    /* The boxes are taller than they look: text is dropped altogether when
       its box is lower than one line of its font, so they overlap a little. */
    text(x0 + pak->width, MEMORY_Y - 6, MEMORY_TEXT, 24, ALIGN_RIGHT, STL_GRAY, expanded ? "8MB" : "4MB");
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = MEMORY_TEXT,
        .height = 22,
        .align = ALIGN_RIGHT,
        .valign = VALIGN_CENTER,
        .style_id = STL_GREEN,
    }, FNT_SMALL, x0 + pak->width, MEMORY_Y + 9, "Detected");
}

void games_ui_topbar_draw (menu_t *menu, games_tab_t selected) {
    static const char *const labels[GAMES_TAB_COUNT] = { "Games", "Recent", "Favorites", "Folders" };
    games_tab_t shown[GAMES_TAB_COUNT];
    int count = tabs_list(shown);

    int x = TABS_X;
    for (int i = 0; i < count; i++) {
        pill(x, TOPBAR_TAB_WIDTH, labels[shown[i]], shown[i] == selected);
        x += TOPBAR_TAB_WIDTH + TOPBAR_TAB_GAP;
    }

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

    /* Not on Folders: the list there starts right under the tab bar. */
    if (features_enabled(FEATURE_MEMORY_BADGE) && selected != GAMES_TAB_FOLDERS) {
        memory_badge_draw();
    }
}

static menu_mode_t origin = MENU_MODE_BROWSER;

void games_ui_set_origin (menu_mode_t tab) {
    origin = tab;
}

menu_mode_t games_ui_origin (void) {
    return origin;
}

#define BADGE_HEIGHT    (20)
#define BADGE_PADDING   (6)
#define BADGE_SPACING   (8)

/* A word in a small box, as wide as its text. Returns where the next one goes. */
int games_ui_badge (int x, int y, const char *label, menu_font_style_t style, bool boxed) {
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

/* The heart that marks a favorite (feature `favorite_heart`): a small white
   shape (rom:/heart.sprite, 112 bytes) tinted here. */
#define HEART_WIDTH     (16)
#define HEART_HEIGHT    (14)
#define HEART_GAP       (8)

static void heart_draw (int x, int y) {
    static sprite_t *heart = NULL;
    if (!heart) {
        heart = sprite_load("rom:/heart.sprite");
    }
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(0xFF, 0x48, 0x68, 0xFF));
        rdpq_sprite_blit(heart, x, y, NULL);
    rdpq_mode_pop();
}

void games_ui_title_panel_draw (const char *title, const char *detail, const game_facts_t *facts) {
    const theme_t *t = theme_get();
    int x0 = GAMES_UI_CONTENT_X0;
    int x1 = GAMES_UI_CONTENT_X1;
    int line2_y = PANEL_Y + 2 + PANEL_TITLE;

    fill(x0, PANEL_Y, x1, PANEL_Y + PANEL_HEIGHT, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));

    /* With the heart switched on, room for it is always kept after the
       name, so the name doesn't change size when the heart turns up. */
    bool heart = features_enabled(FEATURE_FAVORITE_HEART);
    int title_width = x1 - x0 - 24 - (heart ? (HEART_WIDTH + HEART_GAP) : 0);

    /* The big title font when the name fits on one line in it, the body font otherwise. */
    int title_bytes = strlen(title);
    rdpq_paragraph_t *title_layout = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .width = title_width,
        .height = PANEL_TITLE,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
    }, title_font_pick(title, title_width), title, &title_bytes);
    rdpq_paragraph_render(title_layout, x0 + 12, PANEL_Y + 2);
    int title_end = x0 + 12 + (int) title_layout->bbox.x1;
    rdpq_paragraph_free(title_layout);

    if (heart && facts && facts->favorite) {
        heart_draw(title_end + HEART_GAP, PANEL_Y + 2 + ((PANEL_TITLE - HEART_HEIGHT) / 2));
    }

    if (!facts) {
        text(x0 + 12, line2_y, x1 - x0 - 24, PANEL_LINE, ALIGN_LEFT, STL_GRAY, detail);
        return;
    }

    int x = x0 + 12 - BADGE_PADDING;
    int y = line2_y + (PANEL_LINE - BADGE_HEIGHT) / 2;

    if (facts->players > 0) {
        char players[24];
        snprintf(players, sizeof(players), (facts->players == 1) ? "1 player" : "%d players", facts->players);
        x = games_ui_badge(x, y, players, STL_GRAY, false);
    }
    if (facts->needs_expansion) {
        x = games_ui_badge(x, y, "Needs Expansion Pak", STL_ORANGE, true);
    } else if (facts->likes_expansion) {
        x = games_ui_badge(x, y, "Expansion Pak", STL_GREEN, true);
    }
    if (facts->save_found) {
        x = games_ui_badge(x, y, "Save found", STL_GREEN, true);
    }
    if (facts->favorite && !heart) {
        x = games_ui_badge(x, y, "Favorite", STL_YELLOW, true);
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

/* Button icons (feature `button_icons`): each button in its own color, the
   round ones as a disc and the rest as a longer rounded shape. All of them
   are made from one tiny white disc (rom:/button.sprite, 200 bytes), tinted
   and, for the long shapes, cut in half with a filled middle. */
static color_t button_color (const char *button) {
    if (strcmp(button, "A") == 0 || strcmp(button, "Hold") == 0) return RGBA32(0x1C, 0x54, 0xD8, 0xFF);
    if (strcmp(button, "B") == 0) return RGBA32(0x14, 0x94, 0x34, 0xFF);
    if (strcmp(button, "C") == 0) return RGBA32(0xD0, 0x9C, 0x08, 0xFF);
    if (strcmp(button, "START") == 0) return RGBA32(0xCC, 0x20, 0x28, 0xFF);
    return RGBA32(0x70, 0x70, 0x7A, 0xFF);   /* Z, L, R: grey */
}

static bool button_is_round (const char *button) {
    return (strlen(button) == 1) && (strcmp(button, "Z") != 0);
}

static int text_width (const char *string) {
    int nbytes = strlen(string);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { .height = BADGE_HEIGHT }, FNT_DEFAULT, string, &nbytes);
    int width = (int) (layout->bbox.x1 - layout->bbox.x0);
    rdpq_paragraph_free(layout);
    return width;
}

/* How wide the button part of a hint is. */
static int button_width (const char *button) {
    if (features_enabled(FEATURE_BUTTON_ICONS) && button_is_round(button)) {
        return BADGE_HEIGHT;
    }
    return text_width(button) + (BADGE_PADDING * 2);
}

static void button_icon_draw (int x, int y, const char *button) {
    static sprite_t *disc = NULL;
    if (!disc) {
        disc = sprite_load("rom:/button.sprite");
    }

    int width = button_width(button);
    int half = BADGE_HEIGHT / 2;
    color_t color = button_color(button);

    rdpq_mode_push();
        /* The disc's shades say how much of each pixel is covered, which
           gives it a smooth edge; the color comes from here. */
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(color);
        if (width <= BADGE_HEIGHT) {
            rdpq_sprite_blit(disc, x, y, NULL);
        } else {
            rdpq_sprite_blit(disc, x, y, &(rdpq_blitparms_t) { .s0 = 0, .width = half });
            rdpq_sprite_blit(disc, x + width - half, y, &(rdpq_blitparms_t) { .s0 = half, .width = half });
        }
    rdpq_mode_pop();

    if (width > BADGE_HEIGHT) {
        fill(x + half, y, x + width - half, y + BADGE_HEIGHT, color);
    }

    text(x, y, width, BADGE_HEIGHT, ALIGN_CENTER, STL_WHITE, button);
}

void games_ui_hints_backdrop_draw (void) {
    const theme_t *t = theme_get();
    fill(GAMES_UI_CONTENT_X0 - 8, HINTS_Y - 5, GAMES_UI_CONTENT_X1 + 8, HINTS_Y + (2 * HINT_ROW_HEIGHT) + 1,
        RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
}

int games_ui_hint_width (const char *button, const char *action) {
    return button_width(button) + BADGE_PADDING + text_width(action);
}

int games_ui_hint_draw (int x, int row, const char *button, const char *action) {
    int y = HINTS_Y + (row * HINT_ROW_HEIGHT);

    if (features_enabled(FEATURE_BUTTON_ICONS)) {
        button_icon_draw(x, y, button);
        x += button_width(button) + BADGE_PADDING;
    } else {
        x = games_ui_badge(x, y, button, button_style(button), true) - BADGE_SPACING + BADGE_PADDING;
    }

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
