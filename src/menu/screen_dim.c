/**
 * @file screen_dim.c
 * @brief Dim Screen (see screen_dim.h).
 */

#include <libdragon.h>

#include "animations.h"
#include "menu_options.h"
#include "screen_dim.h"
#include "sound.h"
#include "tv_patterns.h"
#include "volume.h"

#define FADE_MS         (1500)  /* the picture and the music fade down over this long */
#define DIM_LEVEL       (204)   /* how far the picture darkens, of 255: a fifth of its brightness left */
#define MUSIC_LEFT      (0.35f) /* the music's loudness, dimmed */

static bool dimmed = false;
static uint64_t dim_since = 0;
static uint64_t press_seen = 0;     /* animations_last_press_ms() when it dimmed */
static bool music_lowered = false;

const char *screen_dim_name (int value) {
    static const char *names[SCREEN_DIM_COUNT] = { "Off", "After 5 Minutes", "After 10 Minutes", "After 20 Minutes" };
    return (value >= 0 && value < SCREEN_DIM_COUNT) ? names[value] : names[0];
}

static uint64_t dim_wait_ms (void) {
    switch (options_get(OPTION_SCREEN_DIM)) {
        case SCREEN_DIM_5_MIN: return 5 * 60 * 1000;
        case SCREEN_DIM_10_MIN: return 10 * 60 * 1000;
        case SCREEN_DIM_20_MIN: return 20 * 60 * 1000;
        default: return 0;
    }
}

/* A button or the stick held down: someone is there, even without a new press. */
static bool anything_held (void) {
    for (int port = 0; port < 4; port++) {
        if (!joypad_is_connected(port)) {
            continue;
        }
        if (joypad_get_buttons(port).raw || joypad_get_direction(port, JOYPAD_2D_ANY) != JOYPAD_8WAY_NONE) {
            return true;
        }
    }
    return false;
}

static void music_back (void) {
    if (music_lowered) {
        float m = volume_music();
        mixer_ch_set_vol(SOUND_BGM_CHANNEL, m, m);
        music_lowered = false;
    }
}

bool screen_dim_active (void) {
    return dimmed;
}

bool screen_dim_wake (void) {
    if (!dimmed || animations_last_press_ms() == press_seen) {
        return false;
    }
    dimmed = false;
    music_back();
    debugf("screen dim: woken after %d s\n", (int) ((get_ticks_ms() - dim_since) / 1000));
    return true;
}

/* Last thing in every frame, before it goes to the TV. */
static void dim_frame (void) {
    uint64_t now = get_ticks_ms();
    if (!dimmed) {
        uint64_t wait = dim_wait_ms();
        uint64_t pressed = animations_last_press_ms();
        if (wait == 0 || now - pressed < wait || anything_held() || tv_patterns_active()) {
            return;
        }
        dimmed = true;
        dim_since = now;
        press_seen = pressed;
        debugf("screen dim: dimmed after %d minutes alone\n", (int) (wait / 60000));
    }
    if (tv_patterns_active() || dim_wait_ms() == 0) {
        dimmed = false;     /* switched off meanwhile */
        music_back();
        return;
    }

    float t = (float) (now - dim_since) / FADE_MS;
    t = (t < 1.0f) ? t : 1.0f;
    float m = volume_music() * (1.0f - (1.0f - MUSIC_LEFT) * t);
    mixer_ch_set_vol(SOUND_BGM_CHANNEL, m, m);
    music_lowered = true;

    int w = (int) display_get_width(), h = (int) display_get_height();
    rdpq_mode_push();
        rdpq_set_scissor(0, 0, w, h);   /* a screen may have left a smaller window set */
        rdpq_set_mode_standard();
        rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, (uint8_t) (DIM_LEVEL * t)));
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_fill_rectangle(0, 0, w, h);
    rdpq_mode_pop();
}

/* The link sends every rdpq_detach_show() here first (Makefile: --wrap). */
void __real_rdpq_detach_show (void);

void __wrap_rdpq_detach_show (void) {
    dim_frame();
    __real_rdpq_detach_show();
}
