/**
 * @file clip.c
 * @brief Recording a short clip of the menu, for an animated GIF.
 */

#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "clip.h"
#include "menu_options.h"
#include "screenshot.h"
#include "sound.h"
#include "utils/fs.h"

#define FOLDER          "sd:/menu/clips"
#define COUNTDOWN_MS    (3000)

static enum { IDLE, ARMED, COUNTING, RECORDING } state = IDLE;
static uint64_t counting_since;     /* real time */
static int ticks_heard;
static char folder[64];
static int frames, frames_wanted, fps;
static bool half, smooth;
static uint64_t began_real_ms;
static char status[48] = "";

/* ---------- the menu's clock ---------- */

uint64_t __real_get_ticks_ms (void);

static bool fixed_clock = false;
static uint64_t clip_base_ms;       /* the menu's time when the clip began */
static uint64_t clip_us;            /* the clip's own time since */
static int64_t skew_ms = 0;         /* real time the menu has not seen (spent saving frames) */

uint64_t __wrap_get_ticks_ms (void) {
    if (fixed_clock) {
        return clip_base_ms + (clip_us / 1000);
    }
    return __real_get_ticks_ms() - skew_ms;
}

static void clock_fix (void) {
    clip_base_ms = __real_get_ticks_ms() - skew_ms;
    clip_us = 0;
    fixed_clock = true;
}

static void clock_free (void) {
    /* Carry on from the clip's time: the real time spent saving is skipped. */
    skew_ms = (int64_t) __real_get_ticks_ms() - (int64_t) (clip_base_ms + (clip_us / 1000));
    fixed_clock = false;
}

/* ---------- recording ---------- */

const char *clip_fps_name (int value) {
    return (value == CLIP_FPS_30) ? "30 per second" : "15 per second";
}

const char *clip_length_name (int value) {
    static const char *names[CLIP_LENGTH_COUNT] = { "5 seconds", "10 seconds", "15 seconds" };
    return (value >= 0 && value < CLIP_LENGTH_COUNT) ? names[value] : names[1];
}

bool clip_busy (void) {
    return state != IDLE;
}

const char *clip_status (menu_t *menu) {
    (void) menu;
    if (state == ARMED) {
        return "Starts when you leave";
    }
    if (state == COUNTING || state == RECORDING) {
        snprintf(status, sizeof(status), "Recording %d of %d", frames, frames_wanted);
        return status;
    }
    return status[0] ? status : "Ready";
}

void clip_arm (menu_t *menu) {
    (void) menu;
    if (state == RECORDING) {
        return;
    }
    state = (state == ARMED) ? IDLE : ARMED;    /* A again: never mind */
    sound_play_effect(SFX_SETTING);
}

static bool open_folder (void) {
    /* directory_create() returns true when it fails. */
    if (!directory_exists(FOLDER) && directory_create(FOLDER)) {
        return false;
    }
    for (int n = 1; n < 10000; n++) {
        snprintf(folder, sizeof(folder), FOLDER "/clip%04d", n);
        if (!directory_exists(folder)) {
            return !directory_create(folder);
        }
    }
    return false;
}

static void finish (bool ok) {
    clock_free();
    state = IDLE;
    char path[96];
    snprintf(path, sizeof(path), "%s/clip.txt", folder);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "clip 1\nfps %d\nframes %d\nsize %s\n", fps, frames, half ? "320x240" : "640x480");
        ok = (fclose(f) == 0) && ok;
    } else {
        ok = false;
    }
    const char *name = strrchr(folder, '/') + 1;
    snprintf(status, sizeof(status), ok ? "Saved %s" : "Stopped: card error", name);
    debugf("clip: %s, %d frames at %d a second, %d s to record\n", ok ? "saved" : "stopped", frames, fps,
           (int) ((__real_get_ticks_ms() - began_real_ms) / 1000));
    sound_play_effect(ok ? SFX_SETTING : SFX_ERROR);
}

void clip_after_frame (menu_t *menu, surface_t *display) {
    switch (state) {
        case IDLE:
            return;

        case ARMED:
            if (menu->mode != MENU_MODE_SETTINGS_EDITOR) {
                state = COUNTING;
                counting_since = __real_get_ticks_ms();
                ticks_heard = 0;
            }
            return;

        case COUNTING: {
            /* A tick each second, so the player knows when it starts. */
            int second = (int) ((__real_get_ticks_ms() - counting_since) / 1000);
            if (second < 3) {
                if (second >= ticks_heard) {
                    ticks_heard = second + 1;
                    sound_play_effect(SFX_CURSOR);
                }
                return;
            }
            if (!open_folder()) {
                state = IDLE;
                snprintf(status, sizeof(status), "Stopped: card error");
                sound_play_effect(SFX_ERROR);
                return;
            }
            fps = (options_get(OPTION_CLIP_FPS) == CLIP_FPS_30) ? 30 : 15;
            frames_wanted = fps * ((options_get(OPTION_CLIP_LENGTH) + 1) * 5);
            half = options_get(OPTION_CLIP_SIZE) == SCREENSHOT_HALF;
            smooth = options_get(OPTION_CLIP_DITHER) != SCREENSHOT_EXACT;
            frames = 0;
            began_real_ms = __real_get_ticks_ms();
            sound_play_effect(SFX_SETTING);
            debugf("clip: recording %s, %d frames\n", folder, frames_wanted);
            state = RECORDING;
            clock_fix();
            return;     /* the next frame drawn is the first, at the clip's time 0 */
        }

        case RECORDING: {
            if (!display) {
                return;
            }
            char path[96];
            snprintf(path, sizeof(path), "%s/frame%04d.png", folder, frames + 1);
            if (!screenshot_write(path, display, half, smooth)) {
                finish(false);
                return;
            }
            frames++;
            clip_us += 1000000 / fps;
            if (frames >= frames_wanted) {
                finish(true);
            }
            return;
        }
    }
}
