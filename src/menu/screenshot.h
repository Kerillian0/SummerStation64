/**
 * @file screenshot.h
 * @brief Saves the screen as a picture on the SD card.
 *
 * Hold START for a second, or press START on a controller in port 2
 * (feature `screenshots`). With screenshots on, a tap of START still does
 * what START does, but when it is let go. The frame drawn next is written
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

/**
 * Called for each controller's new presses before the menu acts on them;
 * may hold back a START press until it is let go.
 * @return true if this was the screenshot button: the menu then ignores it.
 */
bool screenshot_button (joypad_port_t port, joypad_buttons_t *pressed);

/** Names for Settings. */
const char *screenshot_size_name (int value);
const char *screenshot_dither_name (int value);

/** Called once a frame has been drawn: saves it if a screenshot was asked for. */
void screenshot_after_frame (surface_t *display);

#endif /* SCREENSHOT_H__ */
