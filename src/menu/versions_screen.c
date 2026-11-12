#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libdragon.h>

#include "animations.h"
#include "baked_art.h"
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
#define CLOSE_MS        (200)
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
#define BLUR_W          (80)
#define BLUR_H          (60)
#define BLUR_DARK       (0x68)

typedef enum { CLOSED, OPENING, OPEN, CLOSING, PICKED } state_t;    /* PICKED: drawn one last frame, until the next screen takes over */

static state_t state = CLOSED;
static uint64_t state_since = 0;
static bool launch = false;

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

static uint16_t blur_pixels[BLUR_W * BLUR_H] __attribute__((aligned(16)));
static surface_t blur;
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

bool versions_screen_active (void) {
    return state != CLOSED;
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

void versions_screen_capture (surface_t *d) {
    if (!blur_wanted) {
        return;
    }
    blur_wanted = false;
    if (surface_get_format(d) != FMT_RGBA16 || d->width < BLUR_W * 8 || d->height < BLUR_H * 8) {
        return;     /* the plain background is drawn instead */
    }
    uint64_t began = get_ticks_ms();
    /* Each small pixel: the average of 8 of the 64 under it, read straight
       from memory (the graphics chip wrote them; the cache is left alone:
       letting go of cache lines here crashed in the controller code). Eight
       bytes are read at once, four pixels from one memory fetch: reading
       pixels one by one, a fetch each, took 59 ms. */
    const uint8_t *base = (const uint8_t *) UncachedAddr(d->buffer);
    static uint16_t small[BLUR_W * BLUR_H];
    for (int by = 0; by < BLUR_H; by++) {
        const uint64_t *top = (const uint64_t *) (base + ((by * 8) + 2) * d->stride);
        const uint64_t *low = (const uint64_t *) (base + ((by * 8) + 5) * d->stride);
        for (int bx = 0; bx < BLUR_W; bx++) {
            uint64_t q[2] = { top[bx * 2], low[(bx * 2) + 1] };     /* pixels 0-3 of one line, 4-7 of the other */
            int r = 0, g = 0, b = 0;
            for (int k = 0; k < 2; k++) {
                for (int shift = 0; shift < 64; shift += 16) {
                    uint16_t p = (uint16_t) (q[k] >> shift);
                    r += (p >> 11) & 31; g += (p >> 6) & 31; b += (p >> 1) & 31;
                }
            }
            small[(by * BLUR_W) + bx] = (uint16_t) (((r / 8) << 11) | ((g / 8) << 6) | ((b / 8) << 1) | 1);
        }
    }
    /* Then softened once more with each pixel's neighbours. */
    for (int y = 0; y < BLUR_H; y++) {
        for (int x = 0; x < BLUR_W; x++) {
            int r = 0, g = 0, b = 0, n = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= BLUR_W || yy >= BLUR_H) {
                        continue;
                    }
                    uint16_t p = small[(yy * BLUR_W) + xx];
                    r += (p >> 11) & 31; g += (p >> 6) & 31; b += (p >> 1) & 31; n++;
                }
            }
            blur_pixels[(y * BLUR_W) + x] = (uint16_t) (((r / n) << 11) | ((g / n) << 6) | ((b / n) << 1) | 1);
        }
    }
    data_cache_hit_writeback(blur_pixels, sizeof(blur_pixels));
    blur = surface_make(blur_pixels, FMT_RGBA16, BLUR_W, BLUR_H, BLUR_W * 2);
    blur_ready = true;
    blurred_now = true;
    debugf("versions screen: Games screen blurred in %d ms\n", (int) (get_ticks_ms() - began));
}

static void draw_blur (int dark) {
    if (!blur_ready) {
        ui_components_background_draw();
        return;
    }
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        rdpq_set_prim_color(RGBA32(dark, dark, dark, 0xFF));
        rdpq_tex_blit(&blur, 0, 0, &(rdpq_blitparms_t) {
            .scale_x = (float) DISPLAY_WIDTH / BLUR_W,
            .scale_y = (float) DISPLAY_HEIGHT / BLUR_H,
        });
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
    draw_blur(0xFF - (int) ((0xFF - BLUR_DARK) * e));

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
        games_ui_hint_draw(GAMES_UI_CONTENT_X0, 0, "A", launch ? "Play" : "Game info");
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
