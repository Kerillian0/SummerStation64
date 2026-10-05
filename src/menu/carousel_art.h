/**
 * @file carousel_art.h
 * @brief Box art for the selected cover in the carousel.
 *
 * Only one image is kept in memory at a time, so this is safe on a 4MB
 * console. The art is loaded a moment after the selection stops moving,
 * so scrolling stays smooth.
 *
 * The box can be flipped over to show its back. The front is let go before
 * the back is loaded, so flipping costs no extra memory.
 */

#ifndef CAROUSEL_ART_H__
#define CAROUSEL_ART_H__

#include <stdbool.h>

#include "menu_state.h"

/** Call once per frame while the carousel is shown. */
void carousel_art_update (menu_t *menu);

/**
 * Draw the art scaled to fit inside the given cover area.
 * @return False if there is no art (yet), so the caller draws its placeholder.
 */
bool carousel_art_draw (int x0, int y0, int w, int h);

/**
 * Turn the box over (front <-> back). Does nothing if no art is showing.
 * @return True if a flip started.
 */
bool carousel_art_flip (void);

/**
 * How wide the cover should be drawn right now: 1 normally, shrinking to 0
 * and growing back while the box turns over.
 */
float carousel_art_flip_width (void);

/** Free the art, e.g. when leaving the browser. */
void carousel_art_reset (void);

#endif /* CAROUSEL_ART_H__ */
