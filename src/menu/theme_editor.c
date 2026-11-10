#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "theme_editor.h"
#include "card_themes.h"
#include "fonts.h"
#include "games_ui.h"
#include "letter_wheel.h"
#include "menu_options.h"
#include "safe_file.h"
#include "sound.h"
#include "theme.h"
#include "theme_parse.h"
#include "ui_components.h"
#include "ui_components/constants.h"
#include "utils/fs.h"

/* ---------- what can be changed ---------- */

typedef enum {
    R_TEXT,
    R_TEXT_DIM,
    R_ACCENT,
    R_PANEL,
    R_HIGHLIGHT,
    R_TAB_BAR,
    R_OUTLINES,
    R_BACKGROUND,
    R_COLOR1,
    R_COLOR2,
    R_MIDDLE,
    R_COLOR3,
    R_DIRECTION,
    R_PATTERN,
    R_PATTERN_COLOR,
    R_PATTERN_SIZE,
    R_PATTERN_STRENGTH,
    R_SAVE,
    R_UNDO,
    R_COUNT
} row_t;

static bool open_now = false;
static theme_t edit;            /* the theme as it is being made */
static theme_t opened;          /* as it was when the editor opened (or last saved): Undo All Changes */
static bool dirty = false;      /* changed since then */
static row_t selected = R_TEXT;
static int first_row = 0;

/* A stored background (ocean, some pattern sizes) takes about half a second
   to make, so it is made again only once the changes rest. */
#define REBUILD_REST_MS (400)
static bool rebuild_pending = false;
static uint64_t rebuild_at = 0;

typedef enum { ASK_NONE, ASK_LEAVE, ASK_REPLACE } ask_t;
static ask_t ask = ASK_NONE;
static char save_name[41];
static const char *message = NULL;     /* shown until any button is pressed */
static char message_text[96];

static bool gradient (void) { return edit.bg_type == THEME_BG_GRADIENT; }
static bool ocean (void) { return edit.bg_type == THEME_BG_OCEAN; }
static bool patterned (void) { return edit.pattern != THEME_PATTERN_NONE; }

static bool row_shown (row_t row) {
    switch (row) {
        case R_COLOR2: return gradient() || ocean();
        case R_MIDDLE:
        case R_DIRECTION: return gradient();
        case R_COLOR3: return gradient() && edit.use_color3;
        case R_PATTERN_COLOR:
        case R_PATTERN_SIZE:
        case R_PATTERN_STRENGTH: return patterned();
        default: return true;
    }
}

static color_t *row_color (row_t row) {
    switch (row) {
        case R_TEXT: return &edit.text;
        case R_TEXT_DIM: return &edit.text_dim;
        case R_ACCENT: return &edit.accent;
        case R_PANEL: return &edit.panel;
        case R_HIGHLIGHT: return &edit.highlight;
        case R_TAB_BAR: return &edit.tab_inactive;
        case R_OUTLINES: return &edit.border;
        case R_COLOR1: return &edit.color1;
        case R_COLOR2: return &edit.color2;
        case R_COLOR3: return &edit.color3;
        case R_PATTERN_COLOR: return &edit.pattern_color;
        default: return NULL;
    }
}

static bool row_is_background (row_t row) {
    return row >= R_BACKGROUND && row <= R_PATTERN_STRENGTH;
}

static const char *row_label (row_t row) {
    switch (row) {
        case R_TEXT: return "Text";
        case R_TEXT_DIM: return "Dim text";
        case R_ACCENT: return "Accent";
        case R_PANEL: return "Panels and covers";
        case R_HIGHLIGHT: return "Selected row";
        case R_TAB_BAR: return "Tab bar and badges";
        case R_OUTLINES: return "Outlines";
        case R_BACKGROUND: return "Background";
        case R_COLOR1: return ocean() ? "Water" : gradient() ? "First color" : "Background color";
        case R_COLOR2: return ocean() ? "Foam" : "Second color";
        case R_MIDDLE: return "Middle Color";
        case R_COLOR3: return "Middle color";
        case R_DIRECTION: return "Direction";
        case R_PATTERN: return "Pattern";
        case R_PATTERN_COLOR: return "Pattern color";
        case R_PATTERN_SIZE: return "Pattern size";
        case R_PATTERN_STRENGTH: return "Pattern strength";
        case R_SAVE: return "Save as Theme...";
        case R_UNDO: return "Undo All Changes";
        default: return "";
    }
}

/* ---------- choices ---------- */

static const char *const BACKGROUNDS[] = { "Solid", "Gradient", "Picture", "Ocean" };
static const char *const DIRECTIONS[] = { "Vertical", "Horizontal", "Diagonal", "Radial" };
static const char *const PATTERNS[] = { "None", "Stripes", "Diagonal", "Checker", "Dots", "Grid", "Scanlines" };

/* Pattern sizes the background can be drawn live at, with no stored
   picture (theme.c: the pattern must repeat every 2, 4, ... 64 pixels). */
static const int SIZES_DOUBLE[] = { 2, 4, 8, 16, 32 };         /* stripes, diagonal, checker repeat every 2 x size */
static const int SIZES_SINGLE[] = { 2, 4, 8, 16, 32, 64 };     /* dots, grid */
static const int SIZES_SCAN[] = { 8, 16, 32, 64, 128 };        /* scanlines repeat every size / 2 */

static int pattern_sizes (const int **sizes) {
    switch (edit.pattern) {
        case THEME_PATTERN_DOTS:
        case THEME_PATTERN_GRID: *sizes = SIZES_SINGLE; return 6;
        case THEME_PATTERN_SCANLINES: *sizes = SIZES_SCAN; return 5;
        default: *sizes = SIZES_DOUBLE; return 5;
    }
}

/* The place of the pattern's size among the live ones, rounding up. */
static int size_index (void) {
    const int *sizes;
    int n = pattern_sizes(&sizes);
    for (int i = 0; i < n; i++) {
        if (sizes[i] >= edit.pattern_size) {
            return i;
        }
    }
    return n - 1;
}

static void size_snap (void) {
    const int *sizes;
    pattern_sizes(&sizes);
    edit.pattern_size = sizes[size_index()];
}

static void row_value (row_t row, char *out, size_t size) {
    switch (row) {
        case R_BACKGROUND: snprintf(out, size, "%s", BACKGROUNDS[edit.bg_type]); break;
        case R_MIDDLE: snprintf(out, size, "%s", edit.use_color3 ? "On" : "Off"); break;
        case R_DIRECTION: snprintf(out, size, "%s", DIRECTIONS[edit.direction]); break;
        case R_PATTERN: snprintf(out, size, "%s", PATTERNS[edit.pattern]); break;
        case R_PATTERN_SIZE: snprintf(out, size, "%d", edit.pattern_size); break;
        case R_PATTERN_STRENGTH: snprintf(out, size, "%d%%", edit.pattern_opacity); break;
        default: out[0] = '\0'; break;
    }
}

static int wrap (int v, int count) {
    return (v + count) % count;
}

/* Step a choice row one way (-1 or 1). False if it can't go further. */
static bool row_step (row_t row, int way) {
    switch (row) {
        case R_BACKGROUND: {
            /* Solid, Gradient, Ocean: "image" isn't drawn yet, so it isn't offered. */
            static const theme_bg_type_t ORDER[] = { THEME_BG_SOLID, THEME_BG_GRADIENT, THEME_BG_OCEAN };
            int i = 1;
            for (int k = 0; k < 3; k++) {
                if (ORDER[k] == edit.bg_type) i = k;
            }
            edit.bg_type = ORDER[wrap(i + way, 3)];
            return true;
        }
        case R_MIDDLE:
            edit.use_color3 = !edit.use_color3;
            return true;
        case R_DIRECTION:
            edit.direction = (theme_dir_t) wrap(edit.direction + way, 4);
            return true;
        case R_PATTERN: {
            bool was_none = !patterned();
            edit.pattern = (theme_pattern_t) wrap(edit.pattern + way, 7);
            if (was_none && edit.pattern_opacity == 0) {
                edit.pattern_opacity = 10;      /* so the new pattern shows */
            }
            size_snap();
            return true;
        }
        case R_PATTERN_SIZE: {
            const int *sizes;
            int n = pattern_sizes(&sizes);
            int i = size_index();
            if (sizes[i] != edit.pattern_size) {
                edit.pattern_size = sizes[i];   /* a size from a file that can't be drawn live: onto the nearest */
                return true;
            }
            if (i + way < 0 || i + way >= n) {
                return false;
            }
            edit.pattern_size = sizes[i + way];
            return true;
        }
        case R_PATTERN_STRENGTH: {
            int v = edit.pattern_opacity + (way * 5);
            if (v < 0 || v > 100) {
                return false;
            }
            edit.pattern_opacity = v;
            return true;
        }
        default:
            return false;
    }
}

/* ---------- showing changes ---------- */

static void show_changes (bool background) {
    dirty = true;
    if (!background) {
        theme_preview(&edit, false);
    } else if (theme_drawn_live(&edit)) {
        theme_preview(&edit, true);     /* drawn each frame: free to change */
        rebuild_pending = false;
    } else {
        theme_preview(&edit, false);    /* the rest now, the picture once the changes rest */
        rebuild_pending = true;
        rebuild_at = get_ticks_ms();
    }
    fonts_restyle();
}

/* ---------- colors ---------- */

static color_t hsv_color (int h, int s, int v) {
    float hf = (h % 360) / 60.0f;
    int i = (int) hf;
    float f = hf - i;
    float sv = s / 100.0f;
    float top = v * 2.55f;
    float p = top * (1.0f - sv);
    float q = top * (1.0f - sv * f);
    float t = top * (1.0f - sv * (1.0f - f));
    float r, g, b;
    switch (i) {
        case 0: r = top; g = t; b = p; break;
        case 1: r = q; g = top; b = p; break;
        case 2: r = p; g = top; b = t; break;
        case 3: r = p; g = q; b = top; break;
        case 4: r = t; g = p; b = top; break;
        default: r = top; g = p; b = q; break;
    }
    return RGBA32((int) (r + 0.5f), (int) (g + 0.5f), (int) (b + 0.5f), 0xFF);
}

/* Hue is left as it was for greys, so the sliders don't jump. */
static void color_hsv (color_t c, int *h, int *s, int *v) {
    int max = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
    int min = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
    int delta = max - min;
    *v = (max * 100 + 127) / 255;
    *s = max ? (delta * 100 + max / 2) / max : 0;
    if (delta == 0) {
        return;
    }
    float hue;
    if (max == c.r) {
        hue = 60.0f * (float) (c.g - c.b) / delta;
    } else if (max == c.g) {
        hue = 60.0f * (2.0f + (float) (c.b - c.r) / delta);
    } else {
        hue = 60.0f * (4.0f + (float) (c.r - c.g) / delta);
    }
    if (hue < 0.0f) {
        hue += 360.0f;
    }
    *h = ((int) (hue + 0.5f)) % 360;
}

/* The grid: greys on top, then twelve hues pale, vivid, deep and dark (the
   dark ones suit panels and backgrounds). */
#define GRID_COLS       (12)
#define GRID_ROWS       (5)
static const int GRID_SAT[GRID_ROWS] = { 0, 35, 75, 85, 75 };
static const int GRID_VAL[GRID_ROWS] = { 0, 100, 95, 60, 28 };

static void cell_hsv (int row, int col, int *h, int *s, int *v) {
    if (row == 0) {
        *s = 0;
        *v = 100 - (col * 100) / (GRID_COLS - 1);
        return;
    }
    *h = col * (360 / GRID_COLS);
    *s = GRID_SAT[row];
    *v = GRID_VAL[row];
}

static color_t cell_color (int row, int col) {
    int h = 0, s, v;
    cell_hsv(row, col, &h, &s, &v);
    return hsv_color(h, s, v);
}

/* The color picker, open over the list. Focus rows 0-4 are the grid, then
   the three sliders. */
#define FOCUS_HUE       (GRID_ROWS)
#define FOCUS_SAT       (GRID_ROWS + 1)
#define FOCUS_VAL       (GRID_ROWS + 2)
#define HUE_STEP        (3)
#define SV_STEP         (2)

static color_t *picking = NULL;
static color_t picked_before;
static int focus = 0;
static int grid_col = 0;
static int pick_h, pick_s, pick_v;

static void picker_nearest_cell (void) {
    int best = 1 << 30;
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            color_t k = cell_color(r, c);
            int dr = k.r - picking->r, dg = k.g - picking->g, db = k.b - picking->b;
            int d = dr * dr + dg * dg + db * db;
            if (d < best) {
                best = d;
                focus = r;
                grid_col = c;
            }
        }
    }
}

static void picker_open (color_t *target) {
    picking = target;
    picked_before = *target;
    color_hsv(*target, &pick_h, &pick_s, &pick_v);
    picker_nearest_cell();      /* the cursor starts there, but the color is left as it is until it moves */
}

static void picker_take_cell (void) {
    cell_hsv(focus, grid_col, &pick_h, &pick_s, &pick_v);
    *picking = cell_color(focus, grid_col);
    show_changes(row_is_background(selected));
}

static void picker_take_sliders (void) {
    *picking = hsv_color(pick_h, pick_s, pick_v);
    show_changes(row_is_background(selected));
}

static int clamp_step (int v, int step) {
    v += step;
    return v < 0 ? 0 : (v > 100 ? 100 : v);
}

static void picker_process (menu_t *menu) {
    if (menu->actions.enter) {
        picking = NULL;
        sound_play_effect(SFX_SETTING);
        return;
    }
    if (menu->actions.back) {
        *picking = picked_before;
        show_changes(row_is_background(selected));
        picking = NULL;
        sound_play_effect(SFX_EXIT);
        return;
    }

    int way = menu->actions.go_right ? 1 : (menu->actions.go_left ? -1 : 0);
    if (menu->actions.go_up && focus > 0) {
        if (focus == FOCUS_HUE) {
            picker_nearest_cell();      /* back into the grid where the color is nearest */
        } else if (focus < GRID_ROWS) {
            focus--;
            picker_take_cell();
        } else {
            focus--;
        }
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && focus < FOCUS_VAL) {
        if (focus < GRID_ROWS - 1) {
            focus++;
            picker_take_cell();
        } else {
            focus++;
        }
        sound_play_effect(SFX_CURSOR);
    } else if (way && focus < GRID_ROWS) {
        if (grid_col + way >= 0 && grid_col + way < GRID_COLS) {
            grid_col += way;
            picker_take_cell();
            sound_play_effect(SFX_CURSOR);
        }
    } else if (way) {
        if (focus == FOCUS_HUE) {
            pick_h = wrap(pick_h + way * HUE_STEP, 360);    /* round the rainbow */
        } else if (focus == FOCUS_SAT) {
            pick_s = clamp_step(pick_s, way * SV_STEP);
        } else {
            pick_v = clamp_step(pick_v, way * SV_STEP);
        }
        picker_take_sliders();
    }
}

/* ---------- saving ---------- */

static bool write_theme (const char *name) {
    char folder[128], path[160], temp[168];
    snprintf(folder, sizeof(folder), "%s/%s", CARD_THEMES_DIR, name);
    /* directory_create() returns true when it fails. */
    if (!directory_exists(folder) && directory_create(folder)) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/theme.ini", folder);
    snprintf(temp, sizeof(temp), "%s/theme.ini.tmp", folder);
    theme_t out = edit;
    snprintf(out.name, sizeof(out.name), "%s", name);
    return theme_write_file(&out, temp) && safe_file_replace(temp, path);
}

static void say (const char *text) {
    snprintf(message_text, sizeof(message_text), "%s", text);
    message = message_text;
}

static void save (void) {
    if (!write_theme(save_name)) {
        say("Couldn't write the theme to the SD card.");
        sound_play_effect(SFX_ERROR);
        return;
    }
    debugf("theme editor: saved %s/%s/theme.ini\n", CARD_THEMES_DIR, save_name);
    snprintf(edit.name, sizeof(edit.name), "%s", save_name);
    if (card_themes_choose(save_name)) {
        options_flush();
        theme_reload();
        sound_reload_files();   /* a folder saved over keeps its sounds */
        fonts_restyle();
        rebuild_pending = false;
        snprintf(message_text, sizeof(message_text), "Saved as %s. It is your theme now.", save_name);
    } else {
        snprintf(message_text, sizeof(message_text), "Saved as %s in menu/themes.", save_name);  /* more than 32 themes */
    }
    message = message_text;
    opened = edit;
    dirty = false;
    sound_play_effect(SFX_SETTING);
}

/* The letter dial's answer. FAT names can't end in a space or a dot. */
static void named (menu_t *menu, const char *name) {
    (void) menu;
    if (!name) {
        return;
    }
    while (*name == ' ') {
        name++;
    }
    snprintf(save_name, sizeof(save_name), "%s", name);
    int n = strlen(save_name);
    while (n > 0 && (save_name[n - 1] == ' ' || save_name[n - 1] == '.')) {
        save_name[--n] = '\0';
    }
    if (n == 0) {
        say("A theme needs a name.");
        return;
    }
    const char *in_use = card_themes_name_in_use();
    if (card_themes_exists(save_name) && !(in_use && !strcasecmp(in_use, save_name))) {
        ask = ASK_REPLACE;      /* another theme's folder: ask first (saving over the one being edited doesn't) */
        return;
    }
    save();
}

static void begin_save (void) {
    const char *in_use = card_themes_name_in_use();
    char start[41];
    if (in_use) {
        snprintf(start, sizeof(start), "%s", in_use);
    } else {
        snprintf(start, sizeof(start), "My %s", edit.name);     /* a built-in theme: a name of its own */
    }
    letter_wheel_name_begin("Theme name", start, named);
}

/* ---------- the list ---------- */

static void close_editor (void) {
    open_now = false;
    picking = NULL;
    rebuild_pending = false;
    theme_reload();     /* the theme in use again (the saved one, after a save) */
    fonts_restyle();
}

void theme_editor_open (void) {
    edit = *theme_get();
    if (edit.bg_type == THEME_BG_IMAGE) {
        edit.bg_type = THEME_BG_GRADIENT;   /* not drawn yet: shown as a gradient anyway */
    }
    opened = edit;
    dirty = false;
    selected = R_TEXT;
    first_row = 0;
    picking = NULL;
    ask = ASK_NONE;
    message = NULL;
    rebuild_pending = false;
    open_now = true;
}

bool theme_editor_active (void) {
    return open_now;
}

static int shown_rows (row_t *rows) {
    int n = 0;
    for (int r = 0; r < R_COUNT; r++) {
        if (row_shown((row_t) r)) {
            rows[n++] = (row_t) r;
        }
    }
    return n;
}

static int place_of (const row_t *rows, int n, row_t row) {
    for (int i = 0; i < n; i++) {
        if (rows[i] == row) {
            return i;
        }
    }
    return 0;
}

static bool any_button (menu_t *menu) {
    return menu->actions.enter || menu->actions.back || menu->actions.options || menu->actions.settings || menu->actions.lz_context
        || menu->actions.go_up || menu->actions.go_down || menu->actions.go_left || menu->actions.go_right;
}

void theme_editor_process (menu_t *menu) {
    if (rebuild_pending && (get_ticks_ms() - rebuild_at) >= REBUILD_REST_MS) {
        rebuild_pending = false;
        theme_preview(&edit, true);     /* made on the next draw */
    }

    if (message) {
        if (any_button(menu)) {
            message = NULL;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }
    if (ask == ASK_LEAVE) {
        if (menu->actions.enter) {
            ask = ASK_NONE;
            close_editor();
            sound_play_effect(SFX_EXIT);
        } else if (menu->actions.back) {
            ask = ASK_NONE;
            sound_play_effect(SFX_CURSOR);
        }
        return;
    }
    if (ask == ASK_REPLACE) {
        if (menu->actions.enter) {
            ask = ASK_NONE;
            save();
        } else if (menu->actions.back) {
            ask = ASK_NONE;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }
    if (picking) {
        picker_process(menu);
        return;
    }

    row_t rows[R_COUNT];
    int n = shown_rows(rows);
    int i = place_of(rows, n, selected);

    if (menu->actions.go_up && i > 0) {
        selected = rows[i - 1];
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && i < n - 1) {
        selected = rows[i + 1];
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.back) {
        if (dirty) {
            ask = ASK_LEAVE;
            sound_play_effect(SFX_ENTER);
        } else {
            close_editor();
            sound_play_effect(SFX_EXIT);
        }
    } else if (menu->actions.enter && row_color(selected)) {
        picker_open(row_color(selected));
        sound_play_effect(SFX_ENTER);
    } else if (menu->actions.enter && selected == R_SAVE) {
        begin_save();
        sound_play_effect(SFX_ENTER);
    } else if (menu->actions.enter && selected == R_UNDO) {
        if (dirty) {
            edit = opened;
            show_changes(true);
            dirty = false;
            sound_play_effect(SFX_SETTING);
        }
    } else if (menu->actions.enter || menu->actions.go_left || menu->actions.go_right) {
        int way = menu->actions.go_left ? -1 : 1;
        if (row_step(selected, way)) {
            show_changes(true);
            sound_play_effect(SFX_SETTING);
        }
    }
}

/* ---------- drawing ---------- */

#define ROW_HEIGHT      (26)
#define ROW_PADDING     (6)
#define TITLE_Y         (VISIBLE_AREA_Y0 + 10)
#define ROWS_Y          (TITLE_Y + 28)
#define LIST_X          (VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL)
#define LIST_WIDTH      (296)
#define VISIBLE_ROWS    (12)
#define SCROLLBAR_X     (LIST_X + LIST_WIDTH + 3)
#define SWATCH_WIDTH    (34)
#define SWATCH_HEIGHT   (16)
#define HEX_WIDTH       (70)
#define PREVIEW_X0      (LIST_X + LIST_WIDTH + 20)
#define PREVIEW_X1      (VISIBLE_AREA_X1 - TEXT_MARGIN_HORIZONTAL)
#define PREVIEW_MID     ((PREVIEW_X0 + PREVIEW_X1) / 2)

static void fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

/* A frame `thick` pixels wide just inside x0..x1, y0..y1. */
static void frame (int x0, int y0, int x1, int y1, int thick, color_t color) {
    fill(x0, y0, x1, y0 + thick, color);
    fill(x0, y1 - thick, x1, y1, color);
    fill(x0, y0 + thick, x0 + thick, y1 - thick, color);
    fill(x1 - thick, y0 + thick, x1, y1 - thick, color);
}

static void text (int x, int y, int width, rdpq_align_t align, menu_font_style_t style, const char *s) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width,
        .height = ROW_HEIGHT,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, x, y, "%s", s);
}

static void swatch (int x, int y, color_t c) {
    frame(x - 2, y - 2, x + SWATCH_WIDTH + 2, y + SWATCH_HEIGHT + 2, 2, theme_get()->text_dim);
    fill(x, y, x + SWATCH_WIDTH, y + SWATCH_HEIGHT, c);
}

static void draw_row (row_t row, int y, bool chosen) {
    if (chosen) {
        ui_components_box_draw(LIST_X, y, LIST_X + LIST_WIDTH, y + ROW_HEIGHT, theme_get()->highlight);
    }
    bool dim = (row == R_UNDO && !dirty);
    text(LIST_X + ROW_PADDING, y, LIST_WIDTH - ROW_PADDING * 2, ALIGN_LEFT, dim ? STL_GRAY : STL_DEFAULT, row_label(row));

    color_t *c = row_color(row);
    if (c) {
        int sx = LIST_X + LIST_WIDTH - ROW_PADDING - SWATCH_WIDTH - 2;
        swatch(sx, y + (ROW_HEIGHT - SWATCH_HEIGHT) / 2, *c);
        char hex[8];
        snprintf(hex, sizeof(hex), "%02X%02X%02X", c->r, c->g, c->b);
        text(sx - 10 - HEX_WIDTH, y, HEX_WIDTH, ALIGN_RIGHT, STL_GRAY, hex);
    } else {
        char value[24];
        row_value(row, value, sizeof(value));
        text(LIST_X + ROW_PADDING, y, LIST_WIDTH - ROW_PADDING * 2, ALIGN_RIGHT, STL_DEFAULT, value);
    }
}

static void draw_list (void) {
    row_t rows[R_COUNT];
    int n = shown_rows(rows);
    int i = place_of(rows, n, selected);
    if (first_row > i) {
        first_row = i;
    } else if (i >= first_row + VISIBLE_ROWS) {
        first_row = i - VISIBLE_ROWS + 1;
    }
    if (first_row > n - VISIBLE_ROWS) {
        first_row = (n > VISIBLE_ROWS) ? n - VISIBLE_ROWS : 0;    /* rows hidden since: no gap at the end */
    }
    for (int k = first_row; k < n && k < first_row + VISIBLE_ROWS; k++) {
        draw_row(rows[k], ROWS_Y + (k - first_row) * ROW_HEIGHT, k == i);
    }
    if (n > VISIBLE_ROWS) {
        int track = VISIBLE_ROWS * ROW_HEIGHT;
        int thumb = (track * VISIBLE_ROWS) / n;
        int thumb_y = ROWS_Y + ((track - thumb) * first_row) / (n - VISIBLE_ROWS);
        fill(SCROLLBAR_X, ROWS_Y, SCROLLBAR_X + 4, ROWS_Y + track, theme_get()->tab_inactive);
        fill(SCROLLBAR_X, thumb_y, SCROLLBAR_X + 4, thumb_y + thumb, theme_get()->accent);
    }
}

#define CELL            (22)
#define GRID_X          (LIST_X + 4)
#define GRID_Y          (ROWS_Y + 4)
#define SLIDERS_Y       (GRID_Y + GRID_ROWS * CELL + 10)
#define SLIDER_LABEL    (64)
#define SLIDER_SEGMENTS (32)
#define SLIDER_SEG_W    (5)
#define SLIDER_X        (GRID_X + SLIDER_LABEL)
#define SLIDER_W        (SLIDER_SEGMENTS * SLIDER_SEG_W)
#define COMPARE_Y       (SLIDERS_Y + 3 * ROW_HEIGHT + 14)

static void draw_slider (int which, int y) {
    static const char *const LABELS[] = { "Hue", "Color", "Light" };
    bool focused = (focus == FOCUS_HUE + which);
    if (focused) {
        ui_components_box_draw(GRID_X - 4, y, LIST_X + LIST_WIDTH, y + ROW_HEIGHT, theme_get()->highlight);
    }
    text(GRID_X + 2, y, SLIDER_LABEL - 4, ALIGN_LEFT, STL_DEFAULT, LABELS[which]);

    /* The bar shows what each place on it would give. */
    int lit_v = pick_v < 40 ? 40 : pick_v;
    for (int i = 0; i < SLIDER_SEGMENTS; i++) {
        color_t c;
        if (which == 0) {
            c = hsv_color(i * 360 / SLIDER_SEGMENTS, 80, 90);
        } else if (which == 1) {
            c = hsv_color(pick_h, i * 100 / (SLIDER_SEGMENTS - 1), lit_v);
        } else {
            c = hsv_color(pick_h, pick_s, i * 100 / (SLIDER_SEGMENTS - 1));
        }
        int x = SLIDER_X + i * SLIDER_SEG_W;
        fill(x, y + 7, x + SLIDER_SEG_W, y + ROW_HEIGHT - 7, c);
    }
    int value = (which == 0) ? pick_h : (which == 1) ? pick_s : pick_v;
    int max = (which == 0) ? 359 : 100;
    int mx = SLIDER_X + (value * (SLIDER_W - 4)) / max;
    frame(mx - 2, y + 2, mx + 6, y + ROW_HEIGHT - 2, 2, theme_get()->text);

    char number[8];
    snprintf(number, sizeof(number), "%d", value);
    text(SLIDER_X + SLIDER_W + 4, y, LIST_X + LIST_WIDTH - ROW_PADDING - (SLIDER_X + SLIDER_W + 4), ALIGN_RIGHT, STL_DEFAULT, number);
}

static void draw_picker (void) {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            int x = GRID_X + c * CELL;
            int y = GRID_Y + r * CELL;
            fill(x + 1, y + 1, x + CELL - 1, y + CELL - 1, cell_color(r, c));
        }
    }
    if (focus < GRID_ROWS) {
        int x = GRID_X + grid_col * CELL;
        int y = GRID_Y + focus * CELL;
        frame(x - 3, y - 3, x + CELL + 3, y + CELL + 3, 3, theme_get()->text);
    }

    for (int i = 0; i < 3; i++) {
        draw_slider(i, SLIDERS_Y + i * ROW_HEIGHT);
    }

    /* Before and now, side by side. */
    text(GRID_X + 2, COMPARE_Y, 80, ALIGN_LEFT, STL_GRAY, "Before");
    swatch(GRID_X + 84, COMPARE_Y + 5, picked_before);
    text(GRID_X + 140, COMPARE_Y, 50, ALIGN_LEFT, STL_GRAY, "Now");
    swatch(GRID_X + 190, COMPARE_Y + 5, *picking);
    char hex[8];
    snprintf(hex, sizeof(hex), "%02X%02X%02X", picking->r, picking->g, picking->b);
    text(GRID_X + 190 + SWATCH_WIDTH + 8, COMPARE_Y, 70, ALIGN_LEFT, STL_GRAY, hex);
}

/* A small Games screen: the tab bar, three covers with the ring, the title
   panel with a badge, a list with its selected row, and an outlined box. */
static void draw_preview (void) {
    const theme_t *t = theme_get();
    int y = ROWS_Y;

    games_ui_round_fill(PREVIEW_X0, y, PREVIEW_X1, y + 28, t->tab_inactive);
    games_ui_round_fill(PREVIEW_X0 + 4, y + 3, PREVIEW_X0 + 92, y + 25, t->accent);
    text(PREVIEW_X0 + 4, y + 1, 88, ALIGN_CENTER, STL_WHITE, "Games");
    text(PREVIEW_X0 + 100, y + 1, 80, ALIGN_CENTER, STL_DEFAULT, "Folders");

    y += 40;
    int cover_w = 70, cover_h = 96;
    int side_w = 46, side_h = 64;
    int side_y = y + (cover_h - side_h) / 2;
    fill(PREVIEW_X0 + 6, side_y, PREVIEW_X0 + 6 + side_w, side_y + side_h, t->panel);
    fill(PREVIEW_X1 - 6 - side_w, side_y, PREVIEW_X1 - 6, side_y + side_h, t->panel);
    int cx0 = PREVIEW_MID - cover_w / 2;
    fill(cx0, y, cx0 + cover_w, y + cover_h, t->panel);
    frame(cx0 - 6, y - 6, cx0 + cover_w + 6, y + cover_h + 6, 3, t->accent);

    y += cover_h + 16;
    games_ui_round_fill(PREVIEW_X0, y, PREVIEW_X1, y + 66, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = PREVIEW_X1 - PREVIEW_X0 - 20,
        .height = 30,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
    }, FNT_TITLE_MEDIUM, PREVIEW_X0 + 10, y + 2, "Game Title");
    games_ui_badge(PREVIEW_X0 + 10, y + 36, "2 Players", STL_DEFAULT, true);
    text(PREVIEW_X0 + 110, y + 34, PREVIEW_X1 - PREVIEW_X0 - 120, ALIGN_LEFT, STL_GRAY, "1998");

    y += 78;
    ui_components_box_draw(PREVIEW_X0, y, PREVIEW_X1, y + ROW_HEIGHT, t->highlight);
    text(PREVIEW_X0 + ROW_PADDING, y, 200, ALIGN_LEFT, STL_DEFAULT, "Selected row");
    text(PREVIEW_X0 + ROW_PADDING, y + ROW_HEIGHT, 200, ALIGN_LEFT, STL_DEFAULT, "Another row");

    y += ROW_HEIGHT * 2 + 12;
    fill(PREVIEW_X0, y, PREVIEW_X1, y + 34, RGBA32(0x00, 0x00, 0x00, 0xFF));
    frame(PREVIEW_X0, y, PREVIEW_X1, y + 34, 2, t->border);
    text(PREVIEW_X0, y + 4, PREVIEW_X1 - PREVIEW_X0, ALIGN_CENTER, STL_DEFAULT, "Outlined box");
}

void theme_editor_draw (void) {
    text(LIST_X + ROW_PADDING, TITLE_Y, LIST_WIDTH, ALIGN_LEFT, STL_GRAY,
        picking ? row_label(selected) : (dirty ? "EDIT THEME (changed)" : "EDIT THEME"));
    text(PREVIEW_X0, TITLE_Y, PREVIEW_X1 - PREVIEW_X0, ALIGN_LEFT, STL_GRAY, edit.name);

    if (picking) {
        draw_picker();
    } else {
        draw_list();
    }
    draw_preview();

    const char *first;
    if (picking) {
        first = "A: Keep this color";
    } else if (row_color(selected)) {
        first = "A: Pick a color";
    } else if (selected == R_SAVE || selected == R_UNDO) {
        first = "A: Select";
    } else {
        first = "A / Left / Right: Change";
    }
    ui_components_actions_bar_text_draw(STL_DEFAULT, ALIGN_LEFT, VALIGN_TOP, "%s\n%s", first, picking ? "B: Cancel" : "B: Back");

    if (ask == ASK_LEAVE) {
        ui_components_messagebox_draw("Leave without saving?\n\nYour changes will be lost.\n\n^%02XA: Leave^00    ^%02XB: Stay^00", STL_RED, STL_GREEN);
    } else if (ask == ASK_REPLACE) {
        ui_components_messagebox_draw("Replace the theme\n%s?\n\n^%02XA: Replace^00    ^%02XB: Keep it^00", save_name, STL_RED, STL_GREEN);
    } else if (message) {
        ui_components_messagebox_draw("%s\n\nPress any button.", message);
    }
}
