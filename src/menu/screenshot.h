/**
 * @file screenshot.h
 * @brief Saves the screen as a picture on the SD card.
 *
 * Hold START for a second, or press START on a controller in port 2
 * (feature `screenshots`). With screenshots on, a tap of START still does
 * what START does, but when it is let go. Option `shot_button` can ask for
 * L and R held together for a second instead (for controllers whose START
 * is their power button, such as the 8BitDo mod kit): then L and R act when
 * let go, and not at all when they were held together. The frame drawn next is written
 * to sd:/menu/screenshots/shot0001.png and so on, compressed (shot_png.c),
 * at full size (640 x 480) or half (320 x 240), with the 16-bit dither
 * smoothed out or every pixel as the console drew it (options
 * `shot_size`, `shot_dither`).
 */

#ifndef SCREENSHOT_H__
#define SCREENSHOT_H__

#include <stdbool.h>
#include <libdragon.h>

typedef enum { SCREENSHOT_FULL, SCREENSHOT_HALF } screenshot_size_t;
typedef enum { SCREENSHOT_SMOOTH, SCREENSHOT_EXACT } screenshot_dither_t;
typedef enum { SCREENSHOT_BUTTON_START, SCREENSHOT_BUTTON_LR, SCREENSHOT_BUTTON_COUNT } screenshot_button_t;

/**
 * Called for each controller's new presses before the menu acts on them;
 * may hold back a START press until it is let go.
 * @return true if this was the screenshot button: the menu then ignores it.
 */
bool screenshot_button (joypad_port_t port, joypad_buttons_t *pressed);

/**
 * This frame's new presses as the menu sees them, with the screenshot
 * button's held back (noted by the stock button code each frame). Our own
 * code that reads L and R directly uses this instead of the controller.
 */
joypad_buttons_t screenshot_pressed (void);

/** Called by the stock button code with the presses it settled on this frame. */
void screenshot_note_pressed (joypad_buttons_t pressed);

/** Names for Settings. */
const char *screenshot_size_name (int value);
const char *screenshot_dither_name (int value);
const char *screenshot_button_name (int value);

/**
 * Write the frame just drawn as a PNG (also used for clips, clip.h).
 * @param half 320 x 240 instead of 640 x 480.
 * @param smooth Even out the 16-bit dither.
 */
bool screenshot_write (const char *path, surface_t *display, bool half, bool smooth);

/** Called once a frame has been drawn: saves it if a screenshot was asked for. */
void screenshot_after_frame (surface_t *display);

#endif /* SCREENSHOT_H__ */
