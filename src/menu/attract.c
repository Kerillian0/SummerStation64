/**
 * @file attract.c
 * @brief Attract mode: the cover row drifts on its own (see attract.h).
 */

#include <libdragon.h>
#include <string.h>

#include "animations.h"
#include "attract.h"
#include "cover_deal.h"
#include "games_ui.h"
#include "intro.h"
#include "menu_options.h"
#include "theme.h"
#include "ui_components.h"
#include "video_preview.h"

#define GAME_MS         (10000) /* each game takes this long to pass the middle: time for its clip and Vee's word */
#define SETTLE_MS       (240)   /* woken: the row eases into place this long */
#define DIM_MS          (1000)  /* the tab bar, panel and hints dim over this long */
#define DIM_LEVEL       (190)   /* how far they fade into the background, of 255 */
#define LOST_MS         (250)   /* no row asked for this long: it was left, attract mode is over */
#define STEP_MAX_MS     (100)   /* a slow frame moves the covers no further than this */

static bool running = false;
static uint64_t started_ms = 0;
static uint64_t last_ms = 0;        /* the last frame it moved the covers */
static uint64_t asked_ms = 0;       /* the last frame a row asked (attract_frame()) */
static uint64_t press_seen = 0;     /* animations_last_press_ms() when it started */
static float phase = 0.5f;          /* how far the selected game is through its pass: 0 entering, 0.5 in the middle, 1 gone */
static int direction = 1;           /* +1: the selection moves on (covers right to left), -1: back */
static float settle_from = 0.0f;
static uint64_t settle_ms = 0;

const char *attract_wait_name (int value) {
    static const char *names[ATTRACT_COUNT] = { "Off", "After 2 Minutes", "After 5 Minutes", "After 10 Minutes" };
    return (value >= 0 && value < ATTRACT_COUNT) ? names[value] : names[0];
}

const char *attract_direction_name (int value) {
    static const char *names[ATTRACT_DIRECTION_COUNT] = { "Right to Left", "Left to Right" };
    return (value >= 0 && value < ATTRACT_DIRECTION_COUNT) ? names[value] : names[0];
}

static uint64_t attract_wait_ms (void) {
    switch (options_get(OPTION_ATTRACT)) {
        case ATTRACT_2_MIN: return 2 * 60 * 1000;
        case ATTRACT_5_MIN: return 5 * 60 * 1000;
        case ATTRACT_10_MIN: return 10 * 60 * 1000;
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

static float drift (void) {
    return direction * (0.5f - phase);
}

static void stop (bool ease) {
    if (!running) {
        return;
    }
    running = false;
    settle_from = ease ? drift() : 0.0f;
    settle_ms = ease ? get_ticks_ms() : 0;
    debugf("attract: stopped after %d s\n", (int) ((get_ticks_ms() - started_ms) / 1000));
}

bool attract_running (void) {
    if (running && get_ticks_ms() - asked_ms > LOST_MS) {
        stop(false);    /* the row was left (Settings, Game info, another screen) */
    }
    return running;
}

int attract_frame (int entries) {
    uint64_t now = get_ticks_ms();
    attract_running();
    asked_ms = now;
    attract_time_frame();

    if (!running) {
        uint64_t wait = attract_wait_ms();
        uint64_t pressed = animations_last_press_ms();
        if (wait == 0 || entries < 2 || now - pressed < wait || anything_held()
                || cover_deal_running() || intro_fade_dark() > 0 || animations_popup_up()) {
            return 0;
        }
        running = true;
        started_ms = now;
        last_ms = now;
        press_seen = pressed;
        phase = 0.5f;   /* the game in the middle now starts in the middle: no jump */
        direction = (options_get(OPTION_ATTRACT_DIRECTION) == ATTRACT_LEFT_TO_RIGHT) ? -1 : 1;
        settle_ms = 0;
        debugf("attract: started, %d games, %s\n", entries, attract_direction_name(direction > 0 ? 0 : 1));
        return 0;
    }
    if (entries < 2) {
        stop(false);
        return 0;
    }

    uint64_t step = now - last_ms;
    last_ms = now;
    phase += (float) ((step < STEP_MAX_MS) ? step : STEP_MAX_MS) / GAME_MS;
    if (phase >= 1.0f) {
        phase -= 1.0f;
        return direction;
    }
    return 0;
}

float attract_slide (void) {
    if (running) {
        return drift();
    }
    if (settle_ms) {
        float t = (float) (get_ticks_ms() - settle_ms) / SETTLE_MS;
        if (t >= 1.0f) {
            settle_ms = 0;
            return 0.0f;
        }
        return settle_from * (1.0f - t) * (1.0f - t);
    }
    return 0.0f;
}

bool attract_wake (void) {
    if (!attract_running() || animations_last_press_ms() == press_seen) {
        return false;
    }
    stop(true);
    return true;
}

static int dim_now = 0;

/* The background drawn back over each part, part see-through: the tab bar,
   clock and panels fade into it, and the background around them stays as it
   is (darkening the parts' boxes showed as black boxes on the background). */
static void fade_rect (int x0, int y0, int x1, int y1) {
    theme_background_fade_part(x0, y0, x1, y1, dim_now);
}

/* The player's own background picture: darkened instead (the theme's isn't what is under them). */
static void dim_rect (int x0, int y0, int x1, int y1) {
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, dim_now));
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_fill_rectangle(x0 < 0 ? 0 : x0, y0 < 0 ? 0 : y0, x1, y1);
    rdpq_mode_pop();
}

void attract_draw_dim (void) {
    if (!attract_running()) {
        return;
    }
    float t = (float) (get_ticks_ms() - started_ms) / DIM_MS;
    dim_now = (int) (DIM_LEVEL * ((t < 1.0f) ? t : 1.0f));
    if (dim_now > 0) {
        uint64_t began = get_ticks_us();
        games_ui_fade_rects(ui_components_background_get_image() ? dim_rect : fade_rect, false);
        attract_time(ATTRACT_TIME_DIM, (uint32_t) (get_ticks_us() - began));
    }
}

/* Timing, for the log: what each frame spends on what while the covers drift. */
static uint32_t time_sum[ATTRACT_TIME_COUNT];
static uint32_t timed_frames = 0, clip_frames = 0;
static uint64_t timed_since = 0;

void attract_time (attract_time_t part, uint32_t us) {
    if (running && part >= 0 && part < ATTRACT_TIME_COUNT) {
        time_sum[part] += us;
    }
}

void attract_time_frame (void) {
    if (!running) {
        timed_frames = 0;
        return;
    }
    uint64_t now = get_ticks_ms();
    if (timed_frames == 0) {
        memset(time_sum, 0, sizeof(time_sum));
        clip_frames = 0;
        timed_since = now;
    }
    timed_frames++;
    if (video_preview_showing()) {
        clip_frames++;
        time_sum[ATTRACT_TIME_DECODE] += video_preview_stats()->update_us;
    }
    if (now - timed_since >= 2000) {
        float n = timed_frames * 1000.0f;
        debugf("attract: %d frames (%d with a clip), per frame: covers %.1f ms (the clip %.1f), fade %.1f, clip decoding %.1f\n",
               (int) timed_frames, (int) clip_frames, time_sum[ATTRACT_TIME_COVERS] / n, time_sum[ATTRACT_TIME_CLIP] / n,
               time_sum[ATTRACT_TIME_DIM] / n, time_sum[ATTRACT_TIME_DECODE] / n);
        timed_frames = 0;
    }
}
