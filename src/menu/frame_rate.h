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

#endif /* FRAME_RATE_H__ */
