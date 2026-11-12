#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "name_keyboard.h"
#include "fonts.h"
#include "games_ui.h"
#include "menu_features.h"
#include "sound.h"
#include "theme.h"
#include "ui_components/constants.h"

/* ---------- the keys ---------- */

/* Eleven keys a row; the fifth row has ( ) [ ] and a wide Space. The right
   column holds Erase (row 0), Shift (rows 1-2) and OK (rows 3-4). The
   symbols are the dial's: the ones a file name on the SD card may hold. */
#define COLS        (11)
#define ROWS        (5)
#define RIGHT_COL   (COLS)
#define SPACE_COL   (4)     /* the Space key runs from here to the last column */
static const char *const KEYS[ROWS] = {
    "1234567890-",
    "qwertyuiop_",
    "asdfghjkl'+",
    "zxcvbnm,.!&",
    "()[]",
};

typedef enum { SHIFT_OFF, SHIFT_ONCE, SHIFT_CAPS } shift_t;

static int row = 1, col = 0;
static shift_t shift = SHIFT_OFF;

/* B held: after a moment it keeps erasing, quickly. */
#define ERASE_HOLD_MS   (350)
#define ERASE_EVERY_MS  (50)
static uint64_t erase_next = 0;     /* when a held B erases again; 0 while B is up */


static bool on_space (void) {
    return row == ROWS - 1 && col >= SPACE_COL && col < RIGHT_COL;
}

/* The right column's key for a row: 0 Erase, 1 Shift, 2 OK. */
static int right_key (int r) {
    return (r == 0) ? 0 : (r <= 2) ? 1 : 2;
}

static char key_char (int r, int c) {
    char ch = KEYS[r][c];
    return (shift != SHIFT_OFF) ? (char) toupper((unsigned char) ch) : ch;
}

const char *name_keyboard_entry_name (int choice) {
    return (choice == NAME_ENTRY_DIAL) ? "Dial" : "Keyboard";
}

/* ---------- the key labels, laid out once ---------- */

#define KEY_W       (40)
#define KEY_H       (36)
#define KEY_GAP     (4)
#define RIGHT_W     (88)
#define KB_X0       ((DISPLAY_WIDTH - (COLS * KEY_W + RIGHT_W)) / 2)
#define KB_Y0       (226)

/* The 48 key letters are laid out one by one, then put together into one
   text with each letter moved onto its key. Drawn as one, they need one
   setup and few font uploads a frame; drawn as 48 texts (with a box each)
   the keyboard cost about 38 ms a frame (2026-11-10). Made again only when
   Shift changes them. */
static rdpq_paragraph_t *labels = NULL;
static bool labels_upper = false;

static void labels_free (void) {
    free(labels);
    labels = NULL;
}

/* The order the font draws in: by font, then picture (atlas), then style. */
static int draw_order (const void *a, const void *b) {
    uint32_t ka = ((const rdpq_paragraph_char_t *) a)->sort_key & 0xFFFFFF00;
    uint32_t kb = ((const rdpq_paragraph_char_t *) b)->sort_key & 0xFFFFFF00;
    return (ka > kb) - (ka < kb);
}

static void labels_make (void) {
    bool upper = (shift != SHIFT_OFF);
    if (labels && labels_upper == upper) {
        return;
    }
    labels_free();
    int capacity = 0;
    for (int r = 0; r < ROWS; r++) {
        capacity += (int) strlen(KEYS[r]);
    }
    labels = malloc(sizeof(rdpq_paragraph_t) + (capacity + 1) * sizeof(rdpq_paragraph_char_t));
    if (!labels) {
        return;
    }
    memset(labels, 0, sizeof(rdpq_paragraph_t));
    int n = 0;
    for (int r = 0; r < ROWS; r++) {
        int count = (int) strlen(KEYS[r]);
        for (int c = 0; c < count; c++) {
            char text[2] = { key_char(r, c), '\0' };
            int nbytes = 1;
            rdpq_paragraph_t *one = rdpq_paragraph_build(&(rdpq_textparms_t) {
                .width = KEY_W - KEY_GAP,
                .height = KEY_H - KEY_GAP,
                .align = ALIGN_CENTER,
                .valign = VALIGN_CENTER,
                .style_id = STL_DEFAULT,
            }, FNT_DEFAULT, text, &nbytes);
            if (!one) {
                continue;
            }
            int x = KB_X0 + c * KEY_W + (int) one->x0;
            int y = KB_Y0 + r * KEY_H + (int) one->y0;
            for (int i = 0; i < one->nchars && n < capacity; i++) {
                rdpq_paragraph_char_t ch = one->chars[i];
                ch.x += x;
                ch.y += y;
                labels->chars[n++] = ch;
            }
            rdpq_paragraph_free(one);
        }
    }
    qsort(labels->chars, n, sizeof(rdpq_paragraph_char_t), draw_order);
    labels->chars[n].sort_key = 0;      /* the end */
    labels->nchars = n;
    labels->capacity = capacity;
    labels->nlines = ROWS;
    /* Like every text the menu draws: keep the TV's smoothing off the letters. */
    labels->flags = RDPQ_PARAGRAPH_FLAG_ANTIALIAS_FIX;
    labels->bbox.x0 = KB_X0;
    labels->bbox.y0 = KB_Y0;
    labels->bbox.x1 = KB_X0 + COLS * KEY_W;
    labels->bbox.y1 = KB_Y0 + ROWS * KEY_H;
    labels_upper = upper;
}

static sprite_t *corner = NULL;     /* rom:/corner.sprite: a 16x16 disc, for the keys' round corners */

void name_keyboard_begin (const char *text) {
    erase_next = 0;
    row = 1;
    col = 0;
    shift = (text && text[0]) ? SHIFT_OFF : SHIFT_ONCE;     /* a new name starts with a capital, as on the Switch */
}

void name_keyboard_end (void) {
    labels_free();
    if (corner) {
        sprite_free(corner);
        corner = NULL;
    }
}

/* ---------- typing ---------- */

/* A button pressed on any controller this frame (the menu's own button
   reading has no C buttons, R or a plain A). */
static joypad_buttons_t pressed_any (void) {
    joypad_buttons_t all = { 0 };
    JOYPAD_PORT_FOREACH (port) {
        all.raw |= joypad_get_buttons_pressed(port).raw;
    }
    return all;
}

static void type_char (char *text, int *cursor, int limit, char ch) {
    int n = (int) strlen(text);
    if (n >= limit || (ch == ' ' && *cursor == 0)) {
        sound_play_effect(SFX_ERROR);   /* full, or a space at the start (names don't begin with one) */
        return;
    }
    memmove(text + *cursor + 1, text + *cursor, (size_t) (n - *cursor + 1));
    text[(*cursor)++] = ch;
    if (shift == SHIFT_ONCE && isalpha((unsigned char) ch)) {
        shift = SHIFT_OFF;
    }
    sound_play_effect(SFX_SETTING);
}

/* Erase the character before the cursor (nothing at the start: Z gives up, not B). */
static name_keyboard_result_t erase (char *text, int *cursor, bool quiet_at_start) {
    if (*cursor == 0) {
        if (!quiet_at_start) {
            sound_play_effect(SFX_ERROR);
        }
        return NAME_KEYBOARD_TYPING;
    }
    int n = (int) strlen(text);
    memmove(text + *cursor - 1, text + *cursor, (size_t) (n - *cursor + 1));
    (*cursor)--;
    sound_play_effect(SFX_EXIT);
    return NAME_KEYBOARD_TYPING;
}

static bool b_down (void) {
    JOYPAD_PORT_FOREACH (port) {
        if (joypad_get_buttons(port).b) {   /* the state now: "held" is false on the frame it goes down */
            return true;
        }
    }
    return false;
}

static void next_shift (void) {
    shift = (shift == SHIFT_OFF) ? SHIFT_ONCE : (shift == SHIFT_ONCE) ? SHIFT_CAPS : SHIFT_OFF;
    sound_play_effect(SFX_CURSOR);
}

static name_keyboard_result_t finish (const char *text) {
    if (!text[0]) {
        sound_play_effect(SFX_ERROR);   /* nothing typed yet */
        return NAME_KEYBOARD_TYPING;
    }
    return NAME_KEYBOARD_DONE;
}

static void move (menu_t *menu) {
    if (menu->actions.go_left) {
        if (col == RIGHT_COL) {
            col = (row == ROWS - 1) ? SPACE_COL : COLS - 1;
        } else if (on_space()) {
            col = SPACE_COL - 1;
        } else if (col > 0) {
            col--;
        } else {
            col = RIGHT_COL;    /* round to the other side, as on the Switch */
        }
    } else if (menu->actions.go_right) {
        if (on_space() || col == COLS - 1) {
            col = RIGHT_COL;
        } else if (col == RIGHT_COL) {
            col = 0;
        } else {
            col++;
        }
    } else if (menu->actions.go_up || menu->actions.go_down) {
        int way = menu->actions.go_down ? 1 : -1;
        if (col == RIGHT_COL) {
            /* Up and down step between Erase, Shift and OK. */
            static const int first_row[3] = { 0, 1, 3 };
            int k = right_key(row) + way;
            if (k < 0 || k > 2) {
                return;
            }
            row = first_row[k];
        } else {
            int to = row + way;
            if (to < 0 || to >= ROWS) {
                return;
            }
            row = to;
        }
    } else {
        return;
    }
    sound_play_effect(SFX_CURSOR);
}

name_keyboard_result_t name_keyboard_process (menu_t *menu, char *text, int *cursor, int limit) {
    joypad_buttons_t pressed = pressed_any();
    int n = (int) strlen(text);

    if (menu->actions.settings) {   /* START as the menu sees it: one press, also when a tap acts on release (screenshot.h) */
        return finish(text);
    }
    if (pressed.z) {
        sound_play_effect(SFX_EXIT);
        return NAME_KEYBOARD_CANCEL;    /* Z closes it, keeping nothing */
    }
    if (pressed.b) {
        erase_next = get_ticks_ms() + ERASE_HOLD_MS;
        return erase(text, cursor, false);
    }
    if (erase_next) {
        if (!b_down()) {
            erase_next = 0;
        } else if (get_ticks_ms() >= erase_next) {
            erase_next = get_ticks_ms() + ERASE_EVERY_MS;
            return erase(text, cursor, true);
        }
    }
    if (pressed.r) {
        next_shift();
        return NAME_KEYBOARD_TYPING;
    }
    if (pressed.c_down) {
        type_char(text, cursor, limit, ' ');
        return NAME_KEYBOARD_TYPING;
    }
    if (pressed.c_left || pressed.c_right) {
        int to = *cursor + (pressed.c_right ? 1 : -1);
        if (to >= 0 && to <= n) {
            *cursor = to;
            sound_play_effect(SFX_CURSOR);
        }
        return NAME_KEYBOARD_TYPING;
    }
    if (pressed.a) {
        if (col == RIGHT_COL) {
            switch (right_key(row)) {
                case 0: return erase(text, cursor, false);
                case 1: next_shift(); return NAME_KEYBOARD_TYPING;
                default: return finish(text);
            }
        }
        type_char(text, cursor, limit, on_space() ? ' ' : key_char(row, col));
        return NAME_KEYBOARD_TYPING;
    }
    move(menu);
    return NAME_KEYBOARD_TYPING;
}

/* ---------- drawing ---------- */

#define PANEL_X0    (KB_X0 - 14)
#define PANEL_X1    (DISPLAY_WIDTH - PANEL_X0)
#define PANEL_Y0    (92)
#define PANEL_Y1    (448)
#define FIELD_X0    (KB_X0 + 4)
#define FIELD_X1    (DISPLAY_WIDTH - KB_X0 - 4)
#define PROMPT_Y    (PANEL_Y0 + 10)
#define FIELD_Y     (PROMPT_Y + 30)
#define FIELD_H     (34)
#define COUNT_Y     (FIELD_Y + FIELD_H + 6)
#define HINTS_Y     (KB_Y0 + ROWS * KEY_H + 6)
#define FIELD_CHARS (52)    /* a long file name shows the part round the cursor */

static void fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

static void text_at (int x, int y, int w, int h, rdpq_align_t align, menu_font_style_t style, const char *s) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = w,
        .height = h,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_NONE,
        .style_id = style,
    }, FNT_DEFAULT, x, y, "%s", s);
}

/* All the key boxes are drawn in one go, with one drawing mode and one
   upload of the corner disc for the lot: the corners are its quarters, the
   rest is its solid middle, tinted by the prim color. (A rounded box drawn
   on its own sets both up again each time.) */
#define RADIUS      (8)
static bool rounded = true;

static void solid (int x0, int y0, int x1, int y1) {
    if (x1 > x0 && y1 > y0) {
        rdpq_texture_rectangle_raw(TILE0, x0, y0, x1, y1, RADIUS, RADIUS, 0, 0);   /* one texel from the middle of the disc, not stretched */
    }
}

static void box (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_set_prim_color(color);
    int r = rounded ? RADIUS : 0;
    if (r) {
        rdpq_texture_rectangle(TILE0, x0, y0, x0 + r, y0 + r, 0, 0);
        rdpq_texture_rectangle(TILE0, x1 - r, y0, x1, y0 + r, r, 0);
        rdpq_texture_rectangle(TILE0, x0, y1 - r, x0 + r, y1, 0, r);
        rdpq_texture_rectangle(TILE0, x1 - r, y1 - r, x1, y1, r, r);
    }
    solid(x0 + r, y0, x1 - r, y0 + r);
    solid(x0, y0 + r, x1, y1 - r);
    solid(x0 + r, y1 - r, x1 - r, y1);
}

/* One key's box: the selected one lighter and framed, like the Switch's. */
static void key_box (int x0, int y0, int x1, int y1, bool selected, color_t color) {
    const theme_t *t = theme_get();
    box(x0, y0, x1, y1, selected ? t->highlight : color);
    if (selected) {
        rdpq_set_prim_color(t->text);
        solid(x0 - 3, y0 - 3, x1 + 3, y0);
        solid(x0 - 3, y1, x1 + 3, y1 + 3);
        solid(x0 - 3, y0, x0, y1);
        solid(x1, y0, x1 + 3, y1);
    }
}

static void field_draw (const char *text, int cursor) {
    int n = (int) strlen(text);
    int from = cursor - (FIELD_CHARS / 2);
    if (from > n - FIELD_CHARS) from = n - FIELD_CHARS;
    if (from < 0) from = 0;
    char shown[FIELD_CHARS + 8];
    int at = 0;
    if (from > 0) shown[at++] = '<';
    for (int i = from; i <= n && i < from + FIELD_CHARS; i++) {
        if (i == cursor) shown[at++] = '|';
        if (i < n) shown[at++] = text[i];
    }
    if (from + FIELD_CHARS < n) shown[at++] = '>';
    shown[at] = '\0';
    text_at(FIELD_X0 + 6, FIELD_Y, FIELD_X1 - FIELD_X0 - 12, FIELD_H, ALIGN_LEFT, STL_DEFAULT, shown);
    fill(FIELD_X0, FIELD_Y + FIELD_H, FIELD_X1, FIELD_Y + FIELD_H + 2, theme_get()->accent);
}

void name_keyboard_draw (const char *prompt, const char *text, int cursor, int limit) {
    const theme_t *t = theme_get();
    labels_make();
    if (!corner) {
        corner = sprite_load("rom:/corner.sprite");
    }
    rounded = features_enabled(FEATURE_ROUNDED_CORNERS);

    games_ui_round_fill(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xFF));
    text_at(FIELD_X0, PROMPT_Y, FIELD_X1 - FIELD_X0, 26, ALIGN_LEFT, STL_GRAY, prompt);
    field_draw(text, cursor);
    char count[16];
    snprintf(count, sizeof(count), "%d/%d", (int) strlen(text), limit);
    text_at(FIELD_X0, COUNT_Y, FIELD_X1 - FIELD_X0, 26, ALIGN_RIGHT, STL_GRAY, count);

    int sy = KB_Y0 + (ROWS - 1) * KEY_H;
    int sx0 = KB_X0 + SPACE_COL * KEY_W;
    int sx1 = KB_X0 + COLS * KEY_W - KEY_GAP;
    int rx0 = KB_X0 + COLS * KEY_W;
    int rx1 = rx0 + RIGHT_W;
    static const int key_rows[3][2] = { { 0, 1 }, { 1, 3 }, { 3, 5 } };    /* the right keys' first row, and the row after their last */

    /* Every box. */
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_sprite_upload(TILE0, corner, NULL);
        for (int r = 0; r < ROWS; r++) {
            int count_keys = (int) strlen(KEYS[r]);
            for (int c = 0; c < count_keys; c++) {
                int x = KB_X0 + c * KEY_W;
                int y = KB_Y0 + r * KEY_H;
                key_box(x, y, x + KEY_W - KEY_GAP, y + KEY_H - KEY_GAP, row == r && col == c, t->tab_inactive);
            }
        }
        key_box(sx0, sy, sx1, sy + KEY_H - KEY_GAP, on_space(), t->tab_inactive);
        for (int k = 0; k < 3; k++) {
            bool lit = (k == 2) || (k == 1 && shift != SHIFT_OFF);     /* OK stands out; Shift is lit while on */
            key_box(rx0, KB_Y0 + key_rows[k][0] * KEY_H, rx1, KB_Y0 + key_rows[k][1] * KEY_H - KEY_GAP,
                col == RIGHT_COL && right_key(row) == k, lit ? t->accent : t->tab_inactive);
        }
    rdpq_mode_pop();

    /* Every letter. */
    if (labels) {
        rdpq_paragraph_render(labels, 0, 0);
    }

    /* The wide keys' words. */
    text_at(sx0, sy, sx1 - sx0, KEY_H - KEY_GAP, ALIGN_CENTER, STL_DEFAULT, "Space  (C\u25BC)");
    for (int k = 0; k < 3; k++) {
        int y0 = KB_Y0 + key_rows[k][0] * KEY_H;
        int y1 = KB_Y0 + key_rows[k][1] * KEY_H - KEY_GAP;
        bool selected = (col == RIGHT_COL && right_key(row) == k);
        bool lit = (k == 2) || (k == 1 && shift != SHIFT_OFF);
        menu_font_style_t style = (lit && !selected) ? STL_WHITE : STL_DEFAULT;
        if (k == 2) {
            int mid = (y0 + y1) / 2;
            text_at(rx0, mid - 26, RIGHT_W, 26, ALIGN_CENTER, style, "OK");
            text_at(rx0, mid, RIGHT_W, 26, ALIGN_CENTER, style, "START");
        } else {
            text_at(rx0, y0, RIGHT_W, y1 - y0, ALIGN_CENTER, style,
                (k == 0) ? "Erase (B)" : (shift == SHIFT_CAPS) ? "CAPS (R)" : "Shift (R)");
        }
    }

    text_at(PANEL_X0, HINTS_Y, PANEL_X1 - PANEL_X0, 26, ALIGN_CENTER, STL_GRAY,
        "A type  B erase  R shift  C\u25C0\u25B6 move  Z close  START OK");
}
