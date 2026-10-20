/**
 * @file screenshot.h
 * @brief Saves the screen as a picture on the SD card.
 *
 * Press START on a controller in port 2 (feature `screenshots`). That one
 * press is kept from the menu; the frame drawn next is written to
 * sd:/menu/screenshots/shot0001.png and so on, exactly as the console drew
 * it (640 x 480). The picture is written line by line without compression,
 * so it needs almost no memory, and the files are about 900 KB each.
 * A first form, to be refined in v0.3 (one-controller button, smaller files).
 */

#ifndef SCREENSHOT_H__
#define SCREENSHOT_H__

#include <stdbool.h>
#include <libdragon.h>

/**
 * Called for each controller's new presses before the menu acts on them.
 * @return true if this was the screenshot button: the menu then ignores it.
 */
bool screenshot_button (joypad_port_t port, joypad_buttons_t pressed);

/** Called once a frame has been drawn: saves it if a screenshot was asked for. */
void screenshot_after_frame (surface_t *display);

#endif /* SCREENSHOT_H__ */
