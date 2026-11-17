/**
 * @file achievements_ui.c
 * @brief Game info's Achievements page (see achievements_ui.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libdragon.h>

#include "achievements.h"
#include "achievements_ui.h"
#include "fonts.h"
#include "game_info_ui.h"
#include "games_ui.h"
#include "memory_note.h"
#include "png_decoder.h"
#include "screenshot.h"
#include "sound.h"
#include "theme.h"
#include "title_font.h"

#define DIR             "sd:/menu/achievements"
#define X0              (GAMES_UI_CONTENT_X0)
#define X1              (GAMES_UI_CONTENT_X1)

/* Header: the game's icon, its name, how far the player has got. */
#define HEAD_Y          (44)
#define ICON_SIZE       (48)
#define HEAD_TEXT_X     (X0 + ICON_SIZE + 12)
#define BAR_Y           (96)

/* The grid of badges. */
#define BADGE_SIZE      (48)
#define CELL            (56)
#define COLUMNS         (8)
#define ROWS_SHOWN      (3)
#define GRID_X          (X0 + ((X1 - X0) - (COLUMNS * CELL - (CELL - BADGE_SIZE))) / 2)
#define GRID_Y          (112)

/* The selected one's panel. */
#define PANEL_Y0        (GRID_Y + ROWS_SHOWN * CELL + 4)
#define PANEL_Y1        (382)
#define PAD             (10)
#define LINE            (20)

#define SLOTS           (32)
#define ICON_SLOT       (-1)
#define MIN_FREE        (320 * 1024)    /* badges are left out below this (4.5 KB each) */

typedef enum { SLOT_EMPTY, SLOT_WAITING, SLOT_DECODING, SLOT_READY, SLOT_FAILED } slot_state_t;

typedef struct {
    int index;                  /* the achievement, or ICON_SLOT */
    bool locked;                /* which picture: the grey one or the coloured one */
    slot_state_t state;
    surface_t *image;
    uint32_t used;              /* frame last wanted */
} slot_t;

static ach_kind_t kind = ACH_NONE;
static uint32_t game_id = 0;
static bool showing = false;
static bool loaded = false;
static int selected = 0;
static int top_row = 0;
static slot_t slots[SLOTS];
static int decoding = -2;       /* the slot being decoded, -2 none */
static uint32_t frame = 0;

/* The game's badges.pak (pc/ra_bridge/prep.h), open while the page is: one
   file instead of a search through a folder of hundreds per badge. */
static FILE *pak = NULL;
static uint32_t *pak_index = NULL;  /* offset, size per picture */
static int pak_pictures = 0;

/* How long starting each picture took, for the log. */
static int timed = 0;
static uint32_t timed_total_us = 0, timed_worst_us = 0;

/* ---------- badge pictures, one card read at a time ---------- */

static void slot_free (slot_t *s) {
    if (s->state == SLOT_DECODING && decoding == (int) (s - slots)) {
        png_decoder_abort();
        decoding = -2;
    }
    if (s->image) {
        surface_free(s->image);
        free(s->image);
    }
    memset(s, 0, sizeof(*s));
}

static void decoded (png_err_t err, surface_t *image, void *data) {
    int i = (int) (intptr_t) data;
    if (i < 0 || i >= SLOTS || decoding != i) {
        if (image) {
            surface_free(image);
            free(image);
        }
        return;
    }
    decoding = -2;
    slots[i].state = (err == PNG_OK && image) ? SLOT_READY : SLOT_FAILED;
    slots[i].image = (err == PNG_OK) ? image : NULL;
}

/* The slot holding (or about to hold) a picture, or NULL if none is free for it. */
static slot_t *slot_for (int index, bool locked) {
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i].state != SLOT_EMPTY && slots[i].index == index && slots[i].locked == locked) {
            slots[i].used = frame;
            return &slots[i];
        }
    }
    slot_t *oldest = NULL;
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i].state == SLOT_EMPTY) {
            oldest = &slots[i];
            break;
        }
        if (slots[i].used != frame && slots[i].state != SLOT_DECODING && (!oldest || slots[i].used < oldest->used)) {
            oldest = &slots[i];
        }
    }
    if (!oldest) {
        return NULL;
    }
    slot_free(oldest);
    oldest->index = index;
    oldest->locked = locked;
    oldest->state = SLOT_WAITING;
    oldest->used = frame;
    return oldest;
}

static bool memory_ok (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if ((heap.total - heap.used) < MIN_FREE) {
        memory_note_cut(MEMORY_CUT_ACHIEVEMENT_BADGE);
        return false;
    }
    return true;
}

static uint32_t get32 (const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

static void pak_close (void) {
    if (pak) {
        fclose(pak);
    }
    pak = NULL;
    free(pak_index);
    pak_index = NULL;
    pak_pictures = 0;
}

static void pak_open (void) {
    pak_close();
    char path[64];
    snprintf(path, sizeof(path), DIR "/%lu/badges.pak", (unsigned long) game_id);
    pak = fopen(path, "r");
    if (!pak) {
        debugf("achievements: no badges.pak, reading the badge files one by one\n");
        return;
    }
    uint8_t head[8];
    int pictures = achievements_count() * 2 + 1;
    if (fread(head, 1, 8, pak) != 8 || memcmp(head, "BPK1", 4) != 0 || (int) get32(head + 4) != pictures) {
        debugf("achievements: badges.pak doesn't match the list (prepare the card again)\n");
        pak_close();
        return;
    }
    uint8_t *raw = malloc(pictures * 8);
    pak_index = malloc(pictures * 2 * sizeof(uint32_t));
    if (!raw || !pak_index || fread(raw, 1, pictures * 8, pak) != (size_t) (pictures * 8)) {
        free(raw);
        pak_close();
        return;
    }
    for (int i = 0; i < pictures * 2; i++) {
        pak_index[i] = get32(raw + i * 4);
    }
    free(raw);
    pak_pictures = pictures;
}

/* A picture from the pack, started decoding. */
static png_err_t start_from_pak (int picture, int slot) {
    uint32_t offset = pak_index[picture * 2], size = pak_index[picture * 2 + 1];
    if (size == 0 || size > 256 * 1024) {
        return PNG_ERR_NO_FILE;
    }
    uint8_t *buf = malloc(size);
    if (!buf) {
        return PNG_ERR_OUT_OF_MEM;
    }
    if (fseek(pak, offset, SEEK_SET) != 0 || fread(buf, 1, size, pak) != size) {
        free(buf);
        return PNG_ERR_BAD_FILE;
    }
    png_err_t err = png_decoder_start_mem(buf, size, BADGE_SIZE, BADGE_SIZE, decoded, (void *) (intptr_t) slot);
    if (err != PNG_OK) {
        free(buf);      /* only ours again when it wasn't taken */
    }
    return err;
}

/* Start reading one waiting picture, the selected badge first, then the icon, then the rest. */
static void load_one (void) {
    if (decoding != -2) {
        return;
    }
    slot_t *next = NULL;
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i].state == SLOT_WAITING && slots[i].used == frame) {
            if (!next || slots[i].index == selected || (next->index != selected && slots[i].index == ICON_SLOT)) {
                next = &slots[i];
            }
        }
    }
    if (!next) {
        if (timed > 0) {
            debugf("achievements: %d pictures started (%s), %lu us each on average, slowest %lu us\n", timed,
                   pak ? "from badges.pak" : "one file each", (unsigned long) (timed_total_us / timed), (unsigned long) timed_worst_us);
            timed = 0;
            timed_total_us = timed_worst_us = 0;
        }
        return;
    }
    if (!memory_ok()) {
        next->state = SLOT_FAILED;
        return;
    }
    int slot = (int) (next - slots);
    uint32_t started = get_ticks_us();
    png_err_t err;
    if (pak) {
        int picture = (next->index == ICON_SLOT) ? pak_pictures - 1 : next->index * 2 + (next->locked ? 1 : 0);
        err = start_from_pak(picture, slot);
    } else {
        char path[160];
        if (next->index == ICON_SLOT) {
            snprintf(path, sizeof(path), DIR "/%lu/icon.png", (unsigned long) game_id);
        } else {
            const achievement_t *a = achievements_get(next->index);
            snprintf(path, sizeof(path), DIR "/%lu/badges/%s%s.png", (unsigned long) game_id, a ? a->badge : "", next->locked ? "_lock" : "");
        }
        err = png_decoder_start(path, BADGE_SIZE, BADGE_SIZE, decoded, (void *) (intptr_t) slot);
    }
    uint32_t took = get_ticks_us() - started;
    timed++;
    timed_total_us += took;
    if (took > timed_worst_us) {
        timed_worst_us = took;
    }
    if (err == PNG_OK) {
        next->state = SLOT_DECODING;
        decoding = slot;
    } else if (err != PNG_ERR_BUSY) {   /* busy: Game info's own pictures; next frame */
        next->state = SLOT_FAILED;
    }
}

/* ---------- opening and closing ---------- */

void achievements_ui_open (const char *rom_path) {
    achievements_ui_close();
    kind = achievements_for_rom(rom_path, &game_id);
}

void achievements_ui_close (void) {
    for (int i = 0; i < SLOTS; i++) {
        slot_free(&slots[i]);
    }
    decoding = -2;
    pak_close();
    timed = 0;
    timed_total_us = timed_worst_us = 0;
    achievements_set_free();
    kind = ACH_NONE;
    game_id = 0;
    showing = false;
    loaded = false;
    selected = 0;
    top_row = 0;
}

bool achievements_ui_available (void) {
    return kind != ACH_NONE;
}

bool achievements_ui_showing (void) {
    return showing;
}

/* ---------- buttons ---------- */

static void move_to (int index) {
    int count = achievements_count();
    if (count <= 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= count) {
        index = count - 1;
    }
    if (index == selected) {
        return;
    }
    selected = index;
    int row = selected / COLUMNS;
    if (row < top_row) {
        top_row = row;
    } else if (row >= top_row + ROWS_SHOWN) {
        top_row = row - ROWS_SHOWN + 1;
    }
    sound_play_effect(SFX_CURSOR);
}

bool achievements_ui_process (menu_t *menu) {
    bool l = screenshot_pressed().l;
    if (!showing) {
        if (l && kind != ACH_NONE) {
            showing = true;
            if (kind == ACH_SET && !loaded) {
                loaded = achievements_set_load(game_id);
                if (loaded) {
                    pak_open();
                }
            }
            sound_play_effect(SFX_SETTING);
            return true;
        }
        return false;
    }
    if (l) {
        showing = false;
        sound_play_effect(SFX_SETTING);
        return true;
    }
    if (menu->actions.back) {
        showing = false;
        return false;           /* B leaves Game info, as on the info page */
    }
    if (menu->actions.go_left) {
        if (selected % COLUMNS > 0) {
            move_to(selected - 1);
        }
    } else if (menu->actions.go_right) {
        if (selected % COLUMNS < COLUMNS - 1) {
            move_to(selected + 1);
        }
    } else if (menu->actions.go_up) {
        if (selected >= COLUMNS) {
            move_to(selected - COLUMNS);
        }
    } else if (menu->actions.go_down) {
        int count = achievements_count();
        if (selected / COLUMNS < (count - 1) / COLUMNS) {
            move_to(selected + COLUMNS);    /* the last row may be short: to its last badge */
        }
    }
    return true;                /* A, R, Z, START: nothing here */
}

/* ---------- drawing ---------- */

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
        .valign = VALIGN_TOP,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, x, y, "%s", string);
}

static color_t panel_color (void) {
    const theme_t *t = theme_get();
    return RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8);
}

/* A picture, or a plain square while it loads (or if it can't). */
static void picture (int x, int y, int index, bool locked) {
    slot_t *s = slot_for(index, locked);
    if (s && s->state == SLOT_READY && s->image) {
        rdpq_mode_push();
            rdpq_set_mode_copy(false);
            rdpq_tex_blit(s->image, x + (BADGE_SIZE - s->image->width) / 2, y + (BADGE_SIZE - s->image->height) / 2, NULL);
        rdpq_mode_pop();
        return;
    }
    const theme_t *t = theme_get();
    games_ui_round_fill(x, y, x + BADGE_SIZE, y + BADGE_SIZE, RGBA32(t->tab_inactive.r, t->tab_inactive.g, t->tab_inactive.b, 0xFF));
}

static void unlocked_on (char *out, size_t size, uint16_t day) {
    if (day == 0) {
        snprintf(out, size, "Unlocked");
        return;
    }
    time_t when = (time_t) 946684800 + (time_t) day * 86400;     /* days since 2000-01-01 */
    struct tm *tm = gmtime(&when);
    strftime(out, size, "Unlocked %b %d, %Y", tm);
}

static void header (const char *name, bool progress) {
    if (kind == ACH_SET) {
        picture(X0, HEAD_Y, ICON_SLOT, false);
    }
    int x = (kind == ACH_SET) ? HEAD_TEXT_X : X0;
    uint8_t font = title_font_pick(name, X1 - x);
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = X1 - x, .height = 30, .valign = VALIGN_TOP, .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT,
    }, font, x, HEAD_Y, "%s", name);
    if (!progress) {
        return;
    }
    int count, points, total;
    achievements_progress(&count, &points, &total);
    char line[96];
    snprintf(line, sizeof(line), "%d of %d unlocked \xC2\xB7 %d of %d points", count, achievements_count(), points, total);
    text(x, HEAD_Y + 30, X1 - x, LINE, ALIGN_LEFT, STL_GRAY, line);
    const theme_t *t = theme_get();
    fill(x, BAR_Y, X1, BAR_Y + 4, t->tab_inactive);
    if (achievements_count() > 0 && count > 0) {
        fill(x, BAR_Y, x + ((X1 - x) * count) / achievements_count(), BAR_Y + 4, t->accent);
    }
}

static void message (const char *heading, const char *body) {
    games_ui_round_fill(X0, GRID_Y, X1, PANEL_Y1, panel_color());
    text(X0 + PAD, GRID_Y + PAD, X1 - X0 - 2 * PAD, LINE, ALIGN_LEFT, STL_DEFAULT, heading);
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = X1 - X0 - 2 * PAD, .height = 5 * LINE, .valign = VALIGN_TOP, .wrap = WRAP_WORD, .style_id = STL_GRAY,
    }, FNT_DEFAULT, X0 + PAD, GRID_Y + PAD + LINE + 6, "%s", body);
}

static void hints (bool choose) {
    games_ui_hints_backdrop_draw();
    int x = games_ui_hint_draw(GAMES_UI_HINTS_X, 0, "B", "Back");
    games_ui_hint_draw(x, 0, "L", "Info");
    if (choose) {
        games_ui_hint_right_draw(0, "\xE2\x97\x80\xE2\x96\xB2\xE2\x96\xBC\xE2\x96\xB6", "Choose");    /* ◀▲▼▶ */
    }
}

static void grid (void) {
    const theme_t *t = theme_get();
    int count = achievements_count();
    for (int row = 0; row < ROWS_SHOWN; row++) {
        for (int column = 0; column < COLUMNS; column++) {
            int i = (top_row + row) * COLUMNS + column;
            if (i >= count) {
                break;
            }
            int x = GRID_X + column * CELL, y = GRID_Y + row * CELL;
            if (i == selected) {
                games_ui_round_fill(x - 4, y - 4, x + BADGE_SIZE + 4, y + BADGE_SIZE + 4, t->accent);
            }
            picture(x, y, i, !achievements_unlocked(i, NULL));
        }
    }
    /* Where the rows shown sit in the whole grid. */
    int rows = (count + COLUMNS - 1) / COLUMNS;
    if (rows > ROWS_SHOWN) {
        int track_x = X1 - 4, track_y0 = GRID_Y - 4, track_y1 = GRID_Y + ROWS_SHOWN * CELL - 8;
        fill(track_x, track_y0, track_x + 4, track_y1, t->tab_inactive);
        int span = track_y1 - track_y0;
        int thumb_y0 = track_y0 + (span * top_row) / rows;
        int thumb_y1 = track_y0 + (span * (top_row + ROWS_SHOWN)) / rows;
        fill(track_x, thumb_y0, track_x + 4, thumb_y1, t->accent);
    }
}

static void details (void) {
    const achievement_t *a = achievements_get(selected);
    if (!a) {
        return;
    }
    games_ui_round_fill(X0, PANEL_Y0, X1, PANEL_Y1, panel_color());
    int x = X0 + PAD, y = PANEL_Y0 + PAD;
    uint16_t day = 0;
    bool unlocked = achievements_unlocked(selected, &day);
    picture(x, y, selected, !unlocked);
    int tx = x + BADGE_SIZE + PAD, width = X1 - PAD - tx;

    char points[24];
    snprintf(points, sizeof(points), "%u point%s", (unsigned) a->points, a->points == 1 ? "" : "s");
    text(tx, y, width, LINE, ALIGN_RIGHT, STL_YELLOW, points);
    text(tx, y, width - 90, LINE, ALIGN_LEFT, STL_DEFAULT, a->title);
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width, .height = 2 * LINE, .valign = VALIGN_TOP, .wrap = WRAP_WORD, .style_id = STL_GRAY,
    }, FNT_DEFAULT, tx, y + LINE + 2, "%s", a->description);

    int sy = PANEL_Y1 - PAD - LINE;
    char status[48];
    if (unlocked) {
        unlocked_on(status, sizeof(status), day);
    } else {
        snprintf(status, sizeof(status), "Locked");
    }
    text(tx, sy, 190, LINE, ALIGN_LEFT, unlocked ? STL_GREEN : STL_GRAY, status);
    const char *tag = (a->type == ACH_TYPE_MISSABLE) ? "Missable" : (a->type == ACH_TYPE_PROGRESSION) ? "Progression"
                    : (a->type == ACH_TYPE_WIN) ? "Finishes the game" : NULL;
    if (tag) {
        games_ui_badge(tx + 196, sy, tag, (a->type == ACH_TYPE_MISSABLE) ? STL_ORANGE : STL_BLUE, true);
    }
}

void achievements_ui_draw (menu_t *menu) {
    frame++;
    game_info_ui_backdrop_draw(menu, 0x58, 0xFF);   /* the game's blurred cover, as on the info page */

    if (kind == ACH_OTHER_DUMP) {
        header("Achievements", false);
        message("Not for this copy of the game",
                "RetroAchievements has achievements for this game, but not for this dump of it. Its page "
                "for the game lists the dumps it supports; with one of those on the card, run "
                "ra_bridge.exe --prepare again.");
        hints(false);
    } else if (!loaded) {
        header("Achievements", false);
        message("The list couldn't be read", "Run ra_bridge.exe --prepare again with this card in the PC's card reader.");
        hints(false);
    } else if (achievements_count() == 0) {
        header(achievements_set_title(), false);
        message("No achievements yet", "RetroAchievements has no finished achievements for this game yet.");
        hints(false);
    } else {
        header(achievements_set_title(), true);
        grid();
        details();
        hints(true);
    }
    load_one();
}
