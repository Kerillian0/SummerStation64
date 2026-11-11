#include <math.h>
#include <libdragon.h>

#include "fonts.h"
#include "games_ui.h"
#include "sound.h"
#include "theme.h"
#include "tv_patterns.h"
#include "ui_components/constants.h"

#define SHOW_MS     (3500)  /* the name and tip stay this long */
#define SLIDE_MS    (350)   /* then slide off the bottom */

/* Overscan and grid. */
#define GRID        (40)    /* 640 x 480 is 16 x 12 squares, the middle on a line */
#define CIRCLE_R    (200.0f)
#define CIRCLE_W    (3.0f)
#define CIRCLE_SEGS (96)

/* The tip panel. */
#define PANEL_W     (480)
#define PANEL_Y     (296)
#define PANEL_H     (104)

typedef enum {
    PATTERN_OVERSCAN,
    PATTERN_BARS,
    PATTERN_COUNT,
} pattern_t;

static const char *names[PATTERN_COUNT] = {
    "Overscan and Grid",
    "Color Bars",
};

/* Style changes inside a text: ^ and the style's number (fonts.h). */
static const char *tips[PATTERN_COUNT] = {
    "^05Edge   ^042.5%   ^035% (the menu stays inside)   ^0110%\n^00Squares should be square and the circle round.",
    "Raise brightness just until the left box under the grey steps shows.",
};

static bool active = false;
static pattern_t pattern = PATTERN_OVERSCAN;
static uint64_t shown_at = 0;

void tv_patterns_open (void) {
    active = true;
    pattern = PATTERN_OVERSCAN;
    shown_at = get_ticks_ms();
}

bool tv_patterns_active (void) {
    return active;
}

void tv_patterns_process (menu_t *menu) {
    if (menu->actions.back) {
        active = false;
        sound_play_effect(SFX_EXIT);
        return;
    }
    if (menu->actions.enter || menu->actions.go_right) {
        pattern = (pattern + 1) % PATTERN_COUNT;
        shown_at = get_ticks_ms();
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_left) {
        pattern = (pattern + PATTERN_COUNT - 1) % PATTERN_COUNT;
        shown_at = get_ticks_ms();
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.options || menu->actions.lz_context || menu->actions.settings
        || menu->actions.go_up || menu->actions.go_down) {
        shown_at = get_ticks_ms();     /* the name and tip again */
    }
}

/* ---------- drawing ---------- */

/* Fill mode: flat boxes, the cheapest the chip draws. */
static void box (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_set_fill_color(color);
    rdpq_fill_rectangle(x0, y0, x1, y1);
}

/* A border `inset_x`, `inset_y` in from the screen's edges: 4 px wide up the
   sides (a composite TV smears thinner upright lines), 2 px (one TV line in
   240p) across. */
static void border (int inset_x, int inset_y, color_t color) {
    int x0 = inset_x, y0 = inset_y, x1 = DISPLAY_WIDTH - inset_x, y1 = DISPLAY_HEIGHT - inset_y;
    box(x0, y0, x1, y0 + 2, color);
    box(x0, y1 - 2, x1, y1, color);
    box(x0, y0, x0 + 4, y1, color);
    box(x1 - 4, y0, x1, y1, color);
}

/* "5%" in each corner, just inside its border, on black so the grid doesn't run through it. */
static void corner_labels (int inset_x, int inset_y, menu_font_style_t style, const char *label) {
    const int w = 34, h = 20;
    int xs[2] = { inset_x + 6, DISPLAY_WIDTH - inset_x - 6 - w };
    int ys[2] = { inset_y + 4, DISPLAY_HEIGHT - inset_y - 4 - h };
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
            rdpq_fill_rectangle(xs[i], ys[j], xs[i] + w, ys[j] + h);
            rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = h, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
                .style_id = style }, FNT_DEFAULT, xs[i], ys[j], "%s", label);
        }
    }
}

static void circle (float cx, float cy, float r, float width, color_t color) {
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rdpq_set_prim_color(color);
    float r0 = r - (width / 2.0f), r1 = r + (width / 2.0f);
    for (int i = 0; i < CIRCLE_SEGS; i++) {
        float a0 = (2.0f * (float) M_PI * i) / CIRCLE_SEGS;
        float a1 = (2.0f * (float) M_PI * (i + 1)) / CIRCLE_SEGS;
        float in0[2] = { cx + r0 * cosf(a0), cy + r0 * sinf(a0) };
        float in1[2] = { cx + r0 * cosf(a1), cy + r0 * sinf(a1) };
        float out0[2] = { cx + r1 * cosf(a0), cy + r1 * sinf(a0) };
        float out1[2] = { cx + r1 * cosf(a1), cy + r1 * sinf(a1) };
        rdpq_triangle(&TRIFMT_FILL, in0, out0, out1);
        rdpq_triangle(&TRIFMT_FILL, in0, out1, in1);
    }
}

static void draw_overscan (void) {
    color_t grid = RGBA32(0x70, 0x70, 0x70, 0xFF);
    color_t white = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);

    rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
    rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    for (int x = GRID; x < DISPLAY_WIDTH; x += GRID) {
        box(x - 1, 0, x + 1, DISPLAY_HEIGHT, (x == DISPLAY_CENTER_X) ? white : grid);
    }
    for (int y = GRID; y < DISPLAY_HEIGHT; y += GRID) {
        box(0, y - 1, DISPLAY_WIDTH, y + 1, (y == DISPLAY_CENTER_Y) ? white : grid);
    }

    /* Each border's own color, the same as its label and the tip's legend. */
    border(0, 0, RGBA32(0xFF, 0x30, 0x30, 0xFF));                                   /* the picture's edge */
    border(DISPLAY_WIDTH / 40, DISPLAY_HEIGHT / 40, RGBA32(0xFF, 0x90, 0x20, 0xFF));  /* 2.5% */
    border(OVERSCAN_WIDTH, OVERSCAN_HEIGHT, RGBA32(0xFF, 0xE0, 0x30, 0xFF));          /* 5%: the menu's own edge */
    border(DISPLAY_WIDTH / 10, DISPLAY_HEIGHT / 10, RGBA32(0x40, 0xE0, 0x40, 0xFF));  /* 10% */

    circle(DISPLAY_CENTER_X, DISPLAY_CENTER_Y, CIRCLE_R, CIRCLE_W, white);

    corner_labels(OVERSCAN_WIDTH, OVERSCAN_HEIGHT, STL_YELLOW, "5%");
    corner_labels(DISPLAY_WIDTH / 10, DISPLAY_HEIGHT / 10, STL_GREEN, "10%");
}

static void draw_bars (void) {
    const int v = 191;      /* 75% bars, as the broadcast ones */
    static const uint8_t bars[7][3] = {
        { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 1 }, { 0, 1, 0 }, { 1, 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 },
    };
    /* Under each bar, the bar the other way round's color or black. */
    static const int8_t under[7] = { 6, -1, 4, -1, 2, -1, 0 };
    const int bars_y1 = 270, under_y1 = 300, steps_y1 = 370;

    rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
    rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    for (int i = 0; i < 7; i++) {
        int x0 = (i * DISPLAY_WIDTH) / 7, x1 = ((i + 1) * DISPLAY_WIDTH) / 7;
        box(x0, 0, x1, bars_y1, RGBA32(bars[i][0] * v, bars[i][1] * v, bars[i][2] * v, 0xFF));
        int u = under[i];
        color_t c = (u < 0) ? RGBA32(0, 0, 0, 0xFF) : RGBA32(bars[u][0] * v, bars[u][1] * v, bars[u][2] * v, 0xFF);
        box(x0, bars_y1, x1, under_y1, c);
    }

    /* Grey steps, black to white in tenths. */
    for (int i = 0; i <= 10; i++) {
        int x0 = (i * DISPLAY_WIDTH) / 11, x1 = ((i + 1) * DISPLAY_WIDTH) / 11;
        int g = (i * 255) / 10;
        box(x0, under_y1, x1, steps_y1, RGBA32(g, g, g, 0xFF));
    }

    /* Black level: three boxes one, two and three of the console's color
       steps above black (it keeps 32 steps a color), on black. */
    for (int i = 0; i < 3; i++) {
        int g = (i + 1) * 8;
        int x0 = 200 + (i * 90);
        box(x0, 386, x0 + 60, 426, RGBA32(g, g, g, 0xFF));
    }
    /* And a white box, for contrast: bright, without blooming. */
    box(470, 386, 530, 426, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
}

static void draw_tip (void) {
    int since = (int) (get_ticks_ms() - shown_at);
    if (since >= SHOW_MS + SLIDE_MS) {
        return;
    }
    int drop = 0;
    if (since > SHOW_MS) {
        float t = (float) (since - SHOW_MS) / SLIDE_MS;
        drop = (int) ((DISPLAY_HEIGHT - PANEL_Y) * t * t) & ~1;     /* speeding up as it goes */
    }
    const theme_t *theme = theme_get();
    int x0 = (DISPLAY_WIDTH - PANEL_W) / 2, y0 = PANEL_Y + drop;
    games_ui_round_fill(x0, y0, x0 + PANEL_W, y0 + PANEL_H, RGBA32(theme->panel.r, theme->panel.g, theme->panel.b, 0xE8));

    int pad = 14, w = PANEL_W - (2 * pad);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = 28, .align = ALIGN_CENTER, .valign = VALIGN_CENTER },
        FNT_TITLE_MEDIUM, x0 + pad, y0 + 6, "%s", names[pattern]);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = 40, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .wrap = WRAP_WORD }, FNT_DEFAULT, x0 + pad, y0 + 36, "%s", tips[pattern]);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = 20, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .style_id = STL_GRAY }, FNT_DEFAULT, x0 + pad, y0 + 78, "A Next pattern     B Back     Z Show this again");
}

void tv_patterns_draw (void) {
    rdpq_mode_push();
    if (pattern == PATTERN_BARS) {
        draw_bars();
    } else {
        draw_overscan();
    }
    rdpq_mode_pop();
    draw_tip();
}
