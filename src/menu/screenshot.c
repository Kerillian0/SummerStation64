/**
 * @file screenshot.c
 * @brief Saves the screen as a picture on the SD card.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "menu_features.h"
#include "menu_options.h"
#include "safe_file.h"
#include "screenshot.h"
#include "shot_png.h"
#include "sound.h"
#include "utils/fs.h"

#define FOLDER      "sd:/menu/screenshots"
#define TEMP_PATH   FOLDER "/shot.tmp"
#define HOLD_MS     (1000)      /* START held this long takes a picture */

static bool wanted = false;
static int last_number = 0;     /* the highest picture number seen or written */

/* START on each controller: when it went down, and whether that hold took a picture. */
static bool start_down[JOYPAD_PORT_COUNT];
static uint64_t start_down_at[JOYPAD_PORT_COUNT];
static bool start_shot[JOYPAD_PORT_COUNT];

/* L and R on each controller, with "Hold L + R": whether each is down, whether
   both were down together since they were last both up (then neither acts),
   since when, and whether that hold took a picture. */
static bool l_down[JOYPAD_PORT_COUNT], r_down[JOYPAD_PORT_COUNT];
static bool lr_together[JOYPAD_PORT_COUNT], lr_shot[JOYPAD_PORT_COUNT];
static uint64_t lr_since[JOYPAD_PORT_COUNT];

static joypad_buttons_t frame_pressed;

joypad_buttons_t screenshot_pressed (void) {
    return frame_pressed;
}

void screenshot_note_pressed (joypad_buttons_t pressed) {
    frame_pressed = pressed;
}

/* "Hold L + R": L and R act when let go, unless they were held together. */
static void lr_button (joypad_port_t port, joypad_buttons_t *pressed) {
    joypad_buttons_t now = joypad_get_buttons(port);
    if (pressed->l) {
        pressed->l = 0;
        l_down[port] = true;
    }
    if (pressed->r) {
        pressed->r = 0;
        r_down[port] = true;
    }
    if (l_down[port] && r_down[port] && now.l && now.r) {
        if (!lr_together[port]) {
            lr_together[port] = true;
            lr_since[port] = get_ticks_ms();
        } else if (!lr_shot[port] && (get_ticks_ms() - lr_since[port]) >= HOLD_MS) {
            lr_shot[port] = true;
            wanted = true;
        }
    }
    if (l_down[port] && !now.l) {
        l_down[port] = false;
        pressed->l = !lr_together[port];
    }
    if (r_down[port] && !now.r) {
        r_down[port] = false;
        pressed->r = !lr_together[port];
    }
    if (!l_down[port] && !r_down[port]) {
        lr_together[port] = false;
        lr_shot[port] = false;
    }
}

const char *screenshot_size_name (int value) {
    return value == SCREENSHOT_HALF ? "Half (320 x 240)" : "Full (640 x 480)";
}

const char *screenshot_button_name (int value) {
    return value == SCREENSHOT_BUTTON_LR ? "Hold L + R" : "Hold START";
}

const char *screenshot_dither_name (int value) {
    return value == SCREENSHOT_EXACT ? "Exact" : "Smooth";
}

/* ---------- the picture's lines ---------- */

/* Smoothing evens out the 16-bit dither: full size by a 1-2-1 blur each way,
   half size by four pixels each way weighted 1-3-3-1. Each screen line is
   summed across once and kept, in one of four places, while the lines below
   it need it. */
typedef struct {
    surface_t *display;
    bool half, smooth;
    uint16_t *pixels;           /* one screen line as stored */
    uint16_t *sums[4];          /* lines summed across, red, green, blue per pixel out */
    int sum_line[4];
} source_t;

static void read_line (source_t *s, int y) {
    const uint8_t *buffer = VirtualUncachedAddr(s->display->buffer);   /* what the graphics chip wrote, not old cache contents */
    memcpy(s->pixels, buffer + (y * s->display->stride), s->display->width * 2);
}

/* 5 bits each of red, green, blue, then 1 of alpha, as 8 bits each. */
static inline void unpack (uint16_t p, int *r, int *g, int *b) {
    int r5 = (p >> 11) & 31, g5 = (p >> 6) & 31, b5 = (p >> 1) & 31;
    *r = (r5 << 3) | (r5 >> 2);
    *g = (g5 << 3) | (g5 >> 2);
    *b = (b5 << 3) | (b5 >> 2);
}

static const uint16_t *summed (source_t *s, int y) {
    int height = s->display->height, width = s->display->width;
    y = (y < 0) ? 0 : ((y >= height) ? height - 1 : y);
    int slot = y & 3;
    if (s->sum_line[slot] == y) {
        return s->sums[slot];
    }
    s->sum_line[slot] = y;
    read_line(s, y);
    uint16_t *out = s->sums[slot];
    int out_width = s->half ? width / 2 : width;
    for (int x = 0; x < out_width; x++) {
        int sum[3] = { 0, 0, 0 };
        static const int full_weights[3] = { 1, 2, 1 }, half_weights[4] = { 1, 3, 3, 1 };
        int first = s->half ? (2 * x) - 1 : x - 1;
        int taps = s->half ? 4 : 3;
        for (int t = 0; t < taps; t++) {
            int at = first + t;
            at = (at < 0) ? 0 : ((at >= width) ? width - 1 : at);
            int r, g, b, weight = s->half ? half_weights[t] : full_weights[t];
            unpack(s->pixels[at], &r, &g, &b);
            sum[0] += r * weight;
            sum[1] += g * weight;
            sum[2] += b * weight;
        }
        out[(x * 3) + 0] = sum[0];
        out[(x * 3) + 1] = sum[1];
        out[(x * 3) + 2] = sum[2];
    }
    return out;
}

static void picture_line (int y, uint8_t *rgb, void *context) {
    source_t *s = context;
    int out_width = s->half ? s->display->width / 2 : s->display->width;
    if (!s->smooth) {
        read_line(s, s->half ? y * 2 : y);
        for (int x = 0; x < out_width; x++) {
            int r, g, b;
            unpack(s->pixels[s->half ? x * 2 : x], &r, &g, &b);
            rgb[(x * 3) + 0] = r;
            rgb[(x * 3) + 1] = g;
            rgb[(x * 3) + 2] = b;
        }
        return;
    }
    const uint16_t *rows[4];
    int taps = s->half ? 4 : 3;
    int first = s->half ? (2 * y) - 1 : y - 1;
    for (int t = 0; t < taps; t++) {
        rows[t] = summed(s, first + t);
    }
    for (int i = 0; i < out_width * 3; i++) {
        int v = s->half
            ? (rows[0][i] + (3 * rows[1][i]) + (3 * rows[2][i]) + rows[3][i] + 32) / 64
            : (rows[0][i] + (2 * rows[1][i]) + rows[2][i] + 8) / 16;
        rgb[i] = (v > 255) ? 255 : v;
    }
}

bool screenshot_write (const char *path, surface_t *display, bool half, bool smooth) {
    int width = display->width, height = display->height;
    if (width > SHOT_PNG_MAX_WIDTH || surface_get_format(display) != FMT_RGBA16) {
        return false;
    }
    source_t s = {
        .display = display,
        .half = half,
        .smooth = smooth,
        .sum_line = { -1, -1, -1, -1 },
    };
    int out_width = s.half ? width / 2 : width;
    s.pixels = malloc(width * 2);
    uint16_t *sums = s.smooth ? malloc(4 * out_width * 3 * sizeof(uint16_t)) : NULL;
    bool ok = s.pixels && (sums || !s.smooth);
    for (int i = 0; i < 4 && sums; i++) {
        s.sums[i] = sums + (i * out_width * 3);
    }
    FILE *f = ok ? fopen(path, "wb") : NULL;
    if (f) {
        ok = shot_png_write(f, out_width, s.half ? height / 2 : height, picture_line, &s);
        ok = (fclose(f) == 0) && ok;
    } else {
        ok = false;
    }
    free(sums);
    free(s.pixels);
    return ok;
}

bool screenshot_button (joypad_port_t port, joypad_buttons_t *pressed) {
    if (!features_enabled(FEATURE_SCREENSHOTS)) {
        start_down[port] = false;
        l_down[port] = r_down[port] = false;
        return false;
    }
    if (options_get(OPTION_SHOT_BUTTON) == SCREENSHOT_BUTTON_LR) {
        start_down[port] = false;
        lr_button(port, pressed);
        return false;
    }
    l_down[port] = r_down[port] = false;
    /* START counts on release, unless it was held long enough for a picture. */
    bool down = joypad_get_buttons(port).start;     /* the state now: "held" is false on the frame it goes down */
    if (pressed->start) {
        pressed->start = 0;
        start_down[port] = true;
        start_down_at[port] = get_ticks_ms();
        start_shot[port] = false;
    } else if (start_down[port] && down) {
        if (!start_shot[port] && (get_ticks_ms() - start_down_at[port]) >= HOLD_MS) {
            start_shot[port] = true;
            wanted = true;
        }
    } else if (start_down[port]) {
        start_down[port] = false;
        if (!start_shot[port]) {
            pressed->start = 1;     /* a tap: START does what it always does */
        }
    }
    return false;
}

void screenshot_after_frame (surface_t *display) {
    if (!wanted || !display) {
        return;
    }
    wanted = false;
    uint64_t began = get_ticks_ms();

    if (!directory_exists(FOLDER) && directory_create(FOLDER)) {     /* it returns true when it fails */
        debugf("screenshot: could not make %s\n", FOLDER);
        sound_play_effect(SFX_ERROR);
        return;
    }
    char path[64];
    do {
        snprintf(path, sizeof(path), FOLDER "/shot%04d.png", ++last_number);
    } while (file_exists(path) && last_number < 9999);

    bool half = options_get(OPTION_SHOT_SIZE) == SCREENSHOT_HALF;
    bool smooth = options_get(OPTION_SHOT_DITHER) != SCREENSHOT_EXACT;
    if (!screenshot_write(TEMP_PATH, display, half, smooth) || !safe_file_replace(TEMP_PATH, path)) {
        debugf("screenshot: could not save %s\n", path);
        sound_play_effect(SFX_ERROR);
        return;
    }
    debugf("screenshot: saved %s in %d ms\n", path, (int) (get_ticks_ms() - began));
    sound_play_effect(SFX_SETTING);
}
