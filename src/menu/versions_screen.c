#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libdragon.h>

#include "animations.h"
#include "baked_art.h"
#include "carousel_art.h"
#include "controls.h"
#include "fonts.h"
#include "game_index.h"
#include "games_ui.h"
#include "menu_features.h"
#include "menu_options.h"
#include "play_stats.h"
#include "sound.h"
#include "theme.h"
#include "versions_screen.h"
#include "path.h"
#include "ui_components.h"
#include "ui_components/constants.h"

#define MAX_ROWS        (12)
#define OPEN_MS         (300)
#define CLOSE_MS        (250)   /* the column folds back while the Games screen comes back into focus */
#define LAND            (0.25f) /* folding back, the last part of the fold is over the real Games screen (see versions_screen_covers_games()) */
#define MOVE_MS         (160)
#define CARD_MIN_FREE   (384 * 1024)    /* a picture from the card is unpacked only with this much memory free */

/* Where the cover row's selected cover is: the column grows out of it. */
#define FROM_X          (320)
#define FROM_Y          (180)
#define FROM_W          (224)
#define FROM_H          (160)

/* The column. */
#define MIDDLE_Y        (226)
#define BIG_W           (176)
#define BIG_H           (132)
#define SMALL_W         (104)
#define SMALL_H         (76)
#define FIRST_STEP      (118)   /* from the selected cover's middle to its neighbours' */
#define STEP            (88)    /* and on between the small ones */
#define CLIP_Y0         (20)
#define CLIP_Y1         (382)   /* down to the hints */
#define NAMES_X1        (592)
#define DIM_SIDE        (0xB0)  /* covers not selected are a little darker */

/* The blurred Games screen behind. */
#define BLUR_STEPS      (4)             /* halvings: 640x480 to 40x30 */
#define BLUR_W          (DISPLAY_WIDTH >> BLUR_STEPS)
#define BLUR_H          (DISPLAY_HEIGHT >> BLUR_STEPS)
#define BLUR_MIN_FREE   (320 * 1024)    /* the four steps take about 200 KB while the column is open */
#define BLUR_SHIFT_PX   (12.0f)         /* how far four halvings move the picture up and left (measured on a photo) */
#define BLUR_DARK       (0x68)

typedef enum { CLOSED, OPENING, OPEN, CLOSING, PICKED } state_t;    /* PICKED: drawn one last frame, until the next screen takes over */

static state_t state = CLOSED;
static uint64_t state_since = 0;
static bool launch = false;
static versions_pick_t pick_fn = NULL;     /* versions_screen_open_pick(): A hands the version to this instead of opening it */
static const char *pick_word = NULL;

static struct {
    entry_t *entry;
    char name[64];
    char detail[48];
    int art;            /* which picture (arts[]), -1 none */
} rows[MAX_ROWS];
static int count = 0;
static int selected = 0;
static float shown_from = 0.0f;     /* where the column slides from */
static uint64_t moved_at = 0;

static struct {
    char code[5];
    component_boxart_t *art;
    bool tried;
} arts[MAX_ROWS];
static int art_count = 0;

static surface_t steps[BLUR_STEPS];    /* the Games screen halved, again and again: stretched smoothly back, each is blurrier than the one before */
static void free_steps (void);
static bool blur_wanted = false;
static bool blur_ready = false;
static bool blurred_now = false;    /* the blur was made this frame: the covers wait a frame */

const char *versions_screen_layout_name (int choice) {
    return (choice == VERSIONS_LAYOUT_CENTRE) ? "Centre" : "Left";
}

static int column_x (void) {
    return (options_get(OPTION_VERSIONS_LAYOUT) == VERSIONS_LAYOUT_CENTRE) ? DISPLAY_CENTER_X : 152;
}

static int names_x (void) {
    return column_x() + (BIG_W / 2) + 24;
}

static void free_arts (void) {
    if (art_count == 0) {
        return;
    }
    rspq_wait();    /* nothing may still be drawing from them */
    for (int i = 0; i < art_count; i++) {
        baked_art_release(arts[i].art);
        arts[i].art = NULL;
    }
    art_count = 0;
}

void versions_screen_close (void) {
    state = CLOSED;
    blur_ready = blur_wanted = false;
    free_arts();
    if (steps[0].buffer) {
        rspq_wait();    /* the graphics chip may still be using them */
        free_steps();
    }
}

/* "Played 12 times · Oct 26", "Played once · Oct 26, 2025", "Never played". */
static void detail_of (char *out, size_t size, const char *path) {
    static const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    time_t last = 0;
    int times = play_stats_get(path, &last);
    if (times <= 0) {
        snprintf(out, size, "Never played");
        return;
    }
    char when[24] = "";
    if (last > 0) {
        struct tm then = *localtime(&last);
        time_t now_t = time(NULL);
        struct tm now = *localtime(&now_t);
        if (then.tm_year == now.tm_year) {
            snprintf(when, sizeof(when), " · %s %d", months[then.tm_mon], then.tm_mday);
        } else {
            snprintf(when, sizeof(when), " · %s %d, %d", months[then.tm_mon], then.tm_mday, then.tm_year + 1900);
        }
    }
    if (times == 1) {
        snprintf(out, size, "Played once%s", when);
    } else {
        snprintf(out, size, "Played %d times%s", times, when);
    }
}

void versions_screen_open (menu_t *menu, entry_t **entries, const char **names, int n, int first, bool start) {
    versions_screen_close();
    pick_fn = NULL;
    pick_word = NULL;
    count = (n > MAX_ROWS) ? MAX_ROWS : n;
    selected = (first >= 0 && first < count) ? first : 0;
    shown_from = (float) selected;
    moved_at = 0;
    launch = start;
    for (int i = 0; i < count; i++) {
        rows[i].entry = entries[i];
        snprintf(rows[i].name, sizeof(rows[i].name), "%s", names[i]);
        path_t *path = path_clone_push(menu->browser.directory, entries[i]->name);
        detail_of(rows[i].detail, sizeof(rows[i].detail), path_get(path));
        const game_index_entry_t *known = game_index_find(path_get(path), entries[i]->size);
        path_free(path);
        rows[i].art = -1;
        if (!known || !known->code[0]) {
            continue;
        }
        for (int a = 0; a < art_count; a++) {
            if (memcmp(arts[a].code, known->code, 4) == 0) {
                rows[i].art = a;    /* copies of one game share their picture */
                break;
            }
        }
        if (rows[i].art < 0 && art_count < MAX_ROWS) {
            memcpy(arts[art_count].code, known->code, 4);
            arts[art_count].code[4] = 0;
            arts[art_count].art = NULL;
            arts[art_count].tried = false;
            rows[i].art = art_count++;
        }
    }
    blur_wanted = true;
    blur_ready = false;
    state = features_enabled(FEATURE_CAROUSEL_ANIMATION) ? OPENING : OPEN;
    state_since = get_ticks_ms();
    debugf("versions screen: %d versions, %d pictures\n", count, art_count);
}

void versions_screen_open_pick (menu_t *menu, entry_t **entries, const char **names, int n, int selected_first,
                                const char *a_word, versions_pick_t pick) {
    versions_screen_open(menu, entries, names, n, selected_first, false);
    pick_fn = pick;
    pick_word = a_word;
}

bool versions_screen_active (void) {
    return state != CLOSED;
}

static float opened (void);

bool versions_screen_covers_games (void) {
    return state != CLOSED && !(state == CLOSING && opened() < LAND);
}

/* How far it has opened, 0 to 1. */
static float opened (void) {
    int since = (int) (get_ticks_ms() - state_since);
    if (state == OPENING) {
        if (since >= OPEN_MS) {
            state = OPEN;
            return 1.0f;
        }
        return animations_ease_out((float) since / OPEN_MS);
    }
    if (state == CLOSING) {
        float t = (float) since / CLOSE_MS;
        return (t >= 1.0f) ? 0.0f : 1.0f - animations_ease_out(t);
    }
    return 1.0f;
}

/* Where the column is, in rows: slides from where it was to the selected one. */
static float shown (void) {
    if (!moved_at) {
        return (float) selected;
    }
    float t = (float) (get_ticks_ms() - moved_at) / MOVE_MS;
    if (t >= 1.0f) {
        return (float) selected;
    }
    return shown_from + ((float) selected - shown_from) * animations_ease_out(t);
}

static void move_to (int row) {
    shown_from = shown();
    selected = row;
    moved_at = get_ticks_ms();
    sound_play_effect(SFX_CURSOR);
}

bool versions_screen_process (menu_t *menu) {
    if (state == CLOSED) {
        return false;
    }
    if (state == PICKED) {
        return true;
    }
    if (state == CLOSING) {
        if (get_ticks_ms() - state_since >= CLOSE_MS) {
            versions_screen_close();
            carousel_art_open_out();    /* the flat cover has landed on the row's: that one opens out into its box */
        }
        return true;
    }
    bool any = menu->actions.enter || menu->actions.back || menu->actions.go_up || menu->actions.go_down;
    if (state == OPENING && any) {
        state = OPEN;   /* a press finishes the opening */
    }
    if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        if (features_enabled(FEATURE_CAROUSEL_ANIMATION)) {
            state = CLOSING;
            state_since = get_ticks_ms();
        } else {
            versions_screen_close();
        }
        return true;
    }
    if (menu->actions.enter && pick_fn) {
        sound_play_effect(SFX_ENTER);
        pick_fn(menu, rows[selected].entry);    /* e.g. "Hide from the cover tabs?" once the column has folded back */
        if (features_enabled(FEATURE_CAROUSEL_ANIMATION)) {
            state = CLOSING;
            state_since = get_ticks_ms();
        } else {
            versions_screen_close();
        }
        return true;
    }
    if (menu->actions.enter) {
        sound_play_effect(SFX_ENTER);
        menu->browser.entry = rows[selected].entry;
        menu->next_mode = MENU_MODE_LOAD_ROM;
        controls_request_launch(launch);
        state = PICKED;     /* stays on screen (not the Games screen) until Game info opens; the browser closes it then */
        return true;
    }
    if (menu->actions.go_up && selected > 0) {
        move_to(selected - 1);
    } else if (menu->actions.go_down && selected < count - 1) {
        move_to(selected + 1);
    }
    return true;
}

/* ---------- the blurred Games screen ---------- */

/* Halve `from` into `to` (half its size each way). The picture is drawn a
   quarter of a pixel up and left, so the chip samples exactly between four
   pixels, and its mid-texel mode then averages those four: a true halving.
   Sampled on the pixels themselves (no shift), it picked one pixel in four,
   and four steps of that made a jumble, not a blur. */
static void halve (surface_t *from, surface_t *to) {
    rdpq_attach(to, NULL);
    rdpq_set_mode_standard();
    rdpq_mode_filter(FILTER_MEDIAN);
    rdpq_mode_dithering(DITHER_NONE_NONE);
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    rdpq_tex_blit(from, -0.25f, -0.25f, &(rdpq_blitparms_t) { .scale_x = 0.5f, .scale_y = 0.5f });
    rdpq_detach();
}

static void free_steps (void) {
    for (int i = 0; i < BLUR_STEPS; i++) {
        if (steps[i].buffer) {
            surface_free(&steps[i]);
            steps[i].buffer = NULL;
        }
    }
}

void versions_screen_capture (surface_t *d) {
    if (!blur_wanted) {
        return;
    }
    blur_wanted = false;
    if (surface_get_format(d) != FMT_RGBA16 || d->width != DISPLAY_WIDTH || d->height != DISPLAY_HEIGHT) {
        return;     /* the plain background is drawn instead */
    }
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if (heap.total - heap.used < BLUR_MIN_FREE) {
        return;
    }
    /* The graphics chip halves the Games screen still in this buffer four
       times (320x240, 160x120, 80x60, 40x30); stretched smoothly back, the
       last is the blur, and the others the steps in between as the screen
       goes out of focus and back into it (draw_blur()). It reads the old picture before anything new is
       drawn into the buffer, since it does everything in order. The
       processor reading the pixels itself cost 29-59 ms. */
    int w = DISPLAY_WIDTH, h = DISPLAY_HEIGHT;
    surface_t *from = d;
    for (int i = 0; i < BLUR_STEPS; i++) {
        w /= 2;
        h /= 2;
        steps[i] = surface_alloc(FMT_RGBA16, w, h);
        if (!steps[i].buffer) {
            rspq_wait();
            free_steps();
            return;
        }
        halve(from, &steps[i]);
        from = &steps[i];
    }
    blur_ready = true;
    blurred_now = true;
}
/* The Games screen as it was, `sharp` 1 nearly in focus (the 320x240 step),
   down to 0 the full blur (40x30), at brightness `dark`. Each step is one
   solid picture over the whole screen: going out of focus and back costs
   the chip less than the Games screen itself. (A see-through fade over the
   live screen cost 13 ms more a frame and doubled every frame; a dissolve
   was cheap but grainy.) */
static void draw_blur (float sharp, int dark) {
    if (!blur_ready) {
        ui_components_background_draw();
        return;
    }
    int level = BLUR_STEPS - 1 - (int) (sharp * BLUR_STEPS);
    level = (level < 0) ? 0 : (level >= BLUR_STEPS) ? BLUR_STEPS - 1 : level;
    surface_t *step = &steps[level];
    float per_texel = (float) DISPLAY_WIDTH / step->width;
    /* The chip's smoothing places each halving's pixels a little up and to
       the left: about 12 px after four (the owner's photos), growing with
       each, 0.4 px x (2^(n+1) - 2) after n. The picture is moved back down
       and right by that much. */
    int halvings = level + 1;
    float shift = BLUR_SHIFT_PX * (float) ((2 << halvings) - 2) / (float) ((2 << BLUR_STEPS) - 2);
    rdpq_mode_push();
        if (level < BLUR_STEPS - 1) {
            /* Too big for the chip's texture memory at once: drawn in pieces,
               moved by `shift`, with the thin strip it leaves at the top and
               left edges filled. */
            int edge = (int) shift + 1;
            rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
            rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, edge);
            rdpq_fill_rectangle(0, 0, edge, DISPLAY_HEIGHT);
        }
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        rdpq_set_prim_color(RGBA32(dark, dark, dark, 0xFF));
        if (level == BLUR_STEPS - 1) {
            /* The smallest fits whole; its edge pixels carry on past its edges, so the screen's edges stay covered. */
            float back = shift / per_texel;
            rdpq_tex_upload(TILE0, step, NULL);
            rdpq_texture_rectangle_scaled(TILE0, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                -back, -back, step->width - back, step->height - back);
        } else {
            rdpq_tex_blit(step, shift, shift, &(rdpq_blitparms_t) { .scale_x = per_texel, .scale_y = per_texel });
        }
    rdpq_mode_pop();
}

/* ---------- the column ---------- */

static surface_t *picture (int row) {
    int a = rows[row].art;
    if (a < 0 || !arts[a].art || arts[a].art->loading || !arts[a].art->image) {
        return NULL;
    }
    return arts[a].art->image;
}

/* A cover fitted into w x h around (cx, cy), in its art's own shape. */
static void cover_draw (int row, float cx, float cy, float w, float h, bool chosen) {
    const theme_t *t = theme_get();
    surface_t *image = picture(row);
    float fw = w, fh = h, scale = 1.0f;
    if (image && image->width > 0 && image->height > 0) {
        float sx = w / image->width, sy = h / image->height;
        scale = (sx < sy) ? sx : sy;
        fw = image->width * scale;
        fh = image->height * scale;
    }
    int x0 = (int) (cx - fw / 2), y0 = (int) (cy - fh / 2);
    if (chosen && (state == OPEN || state == PICKED)) {
        animations_cover_note(MENU_MODE_BROWSER, x0, y0, x0 + (int) fw, y0 + (int) fh);    /* Game info's cover flies from here */
    }
    if (chosen) {
        games_ui_round_fill(x0 - 5, y0 - 5, x0 + (int) fw + 5, y0 + (int) fh + 5, t->accent);
    }
    if (!image) {
        games_ui_round_fill(x0, y0, x0 + (int) fw, y0 + (int) fh, t->panel);
        return;
    }
    int light = chosen ? 0xFF : DIM_SIDE;
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        rdpq_set_prim_color(RGBA32(light, light, light, 0xFF));
        rdpq_tex_blit(image, x0, y0, &(rdpq_blitparms_t) { .scale_x = scale, .scale_y = scale });
    rdpq_mode_pop();
}

/* A row `o` places from the column's middle: its middle's distance and its size (1 the selected, down to SMALL). */
static void row_place (float o, float *dy, float *grow) {
    float a = (o < 0.0f) ? -o : o;
    float d = (a <= 1.0f) ? a * FIRST_STEP : FIRST_STEP + (a - 1.0f) * STEP;
    *dy = (o < 0.0f) ? -d : d;
    *grow = (a >= 1.0f) ? 0.0f : 1.0f - a;
}

static void text_at (int x, int y, int w, int h, int font_id, menu_font_style_t style, bool wrap, const char *s) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = h, .valign = VALIGN_CENTER,
        .wrap = wrap ? WRAP_WORD : WRAP_ELLIPSES, .style_id = style }, font_id, x, y, "%s", s);
}

void versions_screen_draw (menu_t *menu) {
    (void) menu;
    float e = opened();
    /* Opening, the Games screen goes out of focus and darkens as the column
       grows; folding back, it comes into focus again and is fully bright and
       nearly sharp by the time the fold is three quarters done. For the last
       quarter the browser draws the real Games screen under the column
       instead (its middle cover left out), so the flat cover lands on the
       real screen and the box opens out from it. */
    bool landing = (state == CLOSING && e < LAND);
    if (!landing) {
        float b = (e - LAND) / (1.0f - LAND);
        b = (b < 0.0f) ? 0.0f : (b > 1.0f) ? 1.0f : b;
        draw_blur(1.0f - b, 0xFF - (int) ((0xFF - BLUR_DARK) * b));
    }

    float pos = shown();
    float cx = FROM_X + (column_x() - FROM_X) * e;
    float cy = FROM_Y + (MIDDLE_Y - FROM_Y) * e;
    float big_w = FROM_W + (BIG_W - FROM_W) * e, big_h = FROM_H + (BIG_H - FROM_H) * e;
    int nearest = (int) (pos + 0.5f);

    rdpq_set_scissor(0, CLIP_Y0, DISPLAY_WIDTH, CLIP_Y1);
    /* Farthest first, so nearer covers lie over them while the column folds. */
    for (int far = count; far >= 0; far--) {
        for (int side = -1; side <= 1; side += 2) {
            int i = nearest + (side * far);
            if (far == 0 && side == 1) {
                continue;
            }
            if (i < 0 || i >= count) {
                continue;
            }
            float dy, grow;
            row_place((float) i - pos, &dy, &grow);
            float y = cy + dy * e;
            float w = SMALL_W + (big_w - SMALL_W) * grow, h = SMALL_H + (big_h - SMALL_H) * grow;
            if (y + h / 2 < CLIP_Y0 || y - h / 2 > CLIP_Y1) {
                continue;
            }
            cover_draw(i, cx, y, w, h, i == nearest);
        }
    }

    /* The names, once the column is open. */
    if (state == OPEN || state == PICKED) {
        int nx = names_x(), nw = NAMES_X1 - nx;
        for (int i = 0; i < count; i++) {
            float dy, grow;
            row_place((float) i - pos, &dy, &grow);
            int y = (int) (cy + dy) & ~1;
            if (y < CLIP_Y0 || y > CLIP_Y1) {
                continue;
            }
            if (i == nearest) {
                text_at(nx, y - 46, nw, 54, FNT_TITLE_MEDIUM, STL_DEFAULT, true, rows[i].name);
                text_at(nx, y + 10, nw, 22, FNT_DEFAULT, STL_GRAY, false, rows[i].detail);
            } else {
                text_at(nx, y - 11, nw, 22, FNT_DEFAULT, STL_GRAY, false, rows[i].name);
            }
        }
    }
    rdpq_set_scissor(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    if (state == OPEN || state == PICKED) {
        char heading[24];
        snprintf(heading, sizeof(heading), "%d versions", count);
        text_at(names_x(), CLIP_Y0 + 8, NAMES_X1 - names_x(), 22, FNT_DEFAULT, STL_GRAY, false, heading);
        games_ui_hints_backdrop_draw();
        games_ui_hint_draw(GAMES_UI_CONTENT_X0, 0, "A", pick_word ? pick_word : launch ? "Play" : "Game info");
        games_ui_hint_draw(GAMES_UI_CONTENT_X0, 1, "B", "Back");
        games_ui_hint_right_draw(0, "▲▼", "Choose");
    }
}

/* One picture a frame, the selected version's first, then outward. */
void versions_screen_after_draw (menu_t *menu) {
    if (state != OPENING && state != OPEN) {
        return;
    }
    if (blurred_now) {
        blurred_now = false;    /* that frame had the blur's work: the first cover waits for the next */
        return;
    }
    for (int a = 0; a < art_count; a++) {
        if (arts[a].art && arts[a].art->loading) {
            return;     /* a picture from the card is still unpacking: one at a time */
        }
    }
    for (int far = 0; far < count; far++) {
        for (int side = -1; side <= 1; side += 2) {
            int i = selected + (side * far);
            if (i < 0 || i >= count || rows[i].art < 0 || arts[rows[i].art].tried) {
                continue;
            }
            int a = rows[i].art;
            arts[a].tried = true;
            if (!baked_art_has(arts[a].code, IMAGE_BOXART_FRONT)) {
                heap_stats_t heap;
                sys_get_heap_stats(&heap);
                if (heap.total - heap.used < CARD_MIN_FREE) {
                    return;     /* left as a plain box */
                }
            }
            arts[a].art = baked_art_open(menu->storage_prefix, arts[a].code, NULL, IMAGE_BOXART_FRONT);
            return;
        }
    }
}
