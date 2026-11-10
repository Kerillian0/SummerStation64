/**
 * @file frame_rate.h
 * @brief The menu's frame rate cap: 30 a second (the stock value) or the
 *        display's full rate (60 on NTSC, 50 on PAL).
 *
 * Not offered in Settings: measured on hardware, no screen finishes a frame
 * in the 16.7 ms that 60 needs, so every screen falls back to 30 anyway. The
 * option is still read from options.ini (frame_rate_experiment = 1) for
 * experiments.
 */

#ifndef FRAME_RATE_H__
#define FRAME_RATE_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FRAME_RATE_30,
    FRAME_RATE_FULL,
    FRAME_RATE_COUNT
} frame_rate_t;

/** Label for the Settings screen. */
const char *frame_rate_name (int choice);

/** Apply the player's choice. Call after the display is set up, and after a change. */
void frame_rate_apply (void);

/**
 * Call once per drawn frame, after the screen has drawn itself: waits for
 * the graphics chip to finish the frame, so it is put on screen at once.
 */
void frame_rate_end_frame (void);

/** How long the last frame_rate_end_frame() waited, in microseconds. */
uint64_t frame_rate_last_wait_us (void);

/**
 * Once a frame (menu.c): whether Game info's clip is on screen. With one, an
 * NTSC console runs at every third refresh, a steady 20 a second (the clip's
 * own rate): the page and the clip's decoding together need a little more
 * than every second refresh gives, and missing every other one judders.
 */
void frame_rate_clip (bool showing);

/** How long a frame is meant to take at the cap in use, in microseconds. */
uint32_t frame_rate_frame_us (void);

#endif /* FRAME_RATE_H__ */
