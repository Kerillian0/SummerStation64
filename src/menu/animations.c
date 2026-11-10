#include <libdragon.h>

#include "animations.h"
#include "menu_options.h"
#include "menu_state.h"
#include "ui_components.h"
#include "ui_components/constants.h"

static uint64_t last_press_ms = 0;
static int last_direction = 1;

const char *animations_name (int choice) {
    static const char *names[ANIMATIONS_COUNT] = { "Snappy", "Showy" };
    return (choice >= 0 && choice < ANIMATIONS_COUNT) ? names[choice] : "";
}

bool animations_showy (void) {
    return options_get(OPTION_ANIMATIONS) == ANIMATIONS_SHOWY;
}

void animations_note_press (void) {
    last_press_ms = get_ticks_ms();
}

float animations_progress (uint64_t since, int ms) {
    if (!animations_showy() || last_press_ms > since || ms <= 0) {
        return 1.0f;
    }
    float t = (float) (get_ticks_ms() - since) / ms;
    return (t > 1.0f) ? 1.0f : ((t < 0.0f) ? 0.0f : t);
}

float animations_ease_out (float t) {
    float rest = 1.0f - t;
    return 1.0f - rest * rest * rest;
}

float animations_ease_back (float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

void animations_note_direction (int direction) {
    if (direction != 0) {
        last_direction = (direction > 0) ? 1 : -1;
    }
}

int animations_direction (void) {
    return last_direction;
}

/* ---------- highlights ---------- */

#define GLIDE_MS        (110)
#define GLIDE_FRESH_MS  (150)   /* not drawn for this long: the list was closed, start in place */

static int glide_at (const animations_glide_t *glide) {
    float t = animations_progress(glide->since, GLIDE_MS);
    if (t >= 1.0f) {
        return glide->to;
    }
    return (glide->from + (int) ((glide->to - glide->from) * animations_ease_out(t))) & ~1;    /* even: no shimmer on 240p */
}

int animations_glide (animations_glide_t *glide, int target) {
    uint64_t now = get_ticks_ms();
    bool fresh = !glide->set || (now - glide->last_ms) > GLIDE_FRESH_MS;
    glide->last_ms = now;
    if (fresh || !animations_showy()) {
        glide->set = true;
        glide->from = glide->to = target;
        glide->since = 0;
        return target;
    }
    if (target != glide->to) {
        glide->from = glide_at(glide);
        glide->to = target;
        glide->since = now;
    }
    return glide_at(glide);
}

/* ---------- pop-ups ---------- */

#define DIALOG_OPEN_MS  (150)
#define DIALOG_CLOSE_MS (110)
#define DIALOG_FROM_W   (0.9f)  /* how big it starts (and ends) across */
#define DIALOG_FROM_H   (0.3f)  /* and down: it unrolls from its middle */

static bool drawn_now = false;      /* a pop-up has been drawn this frame */
static bool drawn_prev = false;     /* and last frame */
static int prev_screen = -1;
static uint64_t open_since = 0;
static int last_x0, last_y0, last_x1, last_y1;     /* the last pop-up's box, full size */
static bool clipped = false;        /* growing: what is drawn inside is kept to the box */
static int clip_x0, clip_y0, clip_x1, clip_y1;
static uint64_t close_since = 0;    /* 0: nothing shrinking */
static int close_screen = -1;

static void scaled (int x0, int y0, int x1, int y1, float w, float h, int *out) {
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int half_w = (int) ((x1 - x0) * w / 2.0f), half_h = (int) ((y1 - y0) * h / 2.0f);
    out[0] = cx - half_w;
    out[1] = (cy - half_h) & ~1;
    out[2] = cx + half_w;
    out[3] = (cy + half_h) & ~1;
}

void animations_dialog (int *x0, int *y0, int *x1, int *y1) {
    if (!drawn_now && !drawn_prev) {
        open_since = get_ticks_ms();    /* not up last frame: it has just opened */
        close_since = 0;
    }
    drawn_now = true;
    last_x0 = *x0; last_y0 = *y0; last_x1 = *x1; last_y1 = *y1;
    float t = animations_progress(open_since, DIALOG_OPEN_MS);
    if (t >= 1.0f) {
        return;
    }
    float e = animations_ease_out(t);
    int box[4];
    scaled(*x0, *y0, *x1, *y1, DIALOG_FROM_W + (1.0f - DIALOG_FROM_W) * e, DIALOG_FROM_H + (1.0f - DIALOG_FROM_H) * e, box);
    *x0 = clip_x0 = box[0]; *y0 = clip_y0 = box[1]; *x1 = clip_x1 = box[2]; *y1 = clip_y1 = box[3];
    clipped = true;
}

void animations_dialog_clip (void) {
    if (clipped) {
        rdpq_set_scissor(clip_x0, clip_y0, clip_x1, clip_y1);  /* what is drawn inside shows as the box opens */
    }
}

void animations_dialog_end (void) {
    if (clipped) {
        clipped = false;
        rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
    }
}

void animations_draw_late (int screen) {
    animations_dialog_end();    /* in case a pop-up's drawing didn't */
    if (!drawn_now && drawn_prev && screen == prev_screen && animations_showy()) {
        close_since = get_ticks_ms();
        close_screen = screen;
    }
    if (drawn_now || close_since == 0) {
        return;
    }
    float t = animations_progress(close_since, DIALOG_CLOSE_MS);
    if (t >= 1.0f || screen != close_screen) {
        close_since = 0;
        return;
    }
    float e = t * t;    /* eases away */
    int box[4];
    scaled(last_x0, last_y0, last_x1, last_y1, 1.0f - (1.0f - DIALOG_FROM_W) * e, 1.0f - (1.0f - DIALOG_FROM_H) * e, box);
    ui_components_border_draw(box[0], box[1], box[2], box[3]);
    ui_components_box_draw(box[0], box[1], box[2], box[3], DIALOG_BG_COLOR);
}

/* ---------- the cover between a row and Game info ---------- */

#define COVER_FRESH_MS  (1500)  /* a row reading its list again can take a while */

static struct {
    int screen;
    int rect[4];
    uint64_t ms;
    bool set;
} cover_place;

static bool game_info_screen (int screen) {
    return screen == MENU_MODE_LOAD_ROM || screen == MENU_MODE_LOAD_DISK || screen == MENU_MODE_LOAD_EMULATOR;
}

void animations_cover_note (int screen, int x0, int y0, int x1, int y1) {
    cover_place.screen = screen;
    cover_place.rect[0] = x0; cover_place.rect[1] = y0; cover_place.rect[2] = x1; cover_place.rect[3] = y1;
    cover_place.ms = get_ticks_ms();
    cover_place.set = true;
}

bool animations_cover_take (int screen, int from[4]) {
    if (!cover_place.set || !animations_showy() || game_info_screen(screen) == game_info_screen(cover_place.screen)
        || (get_ticks_ms() - cover_place.ms) > COVER_FRESH_MS) {
        return false;
    }
    cover_place.set = false;
    for (int i = 0; i < 4; i++) {
        from[i] = cover_place.rect[i];
    }
    return true;
}

void animations_frame_done (int screen) {
    drawn_prev = drawn_now;
    drawn_now = false;
    prev_screen = screen;
}
