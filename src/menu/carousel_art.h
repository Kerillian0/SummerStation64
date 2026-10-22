/**
 * @file carousel_art.h
 * @brief Box art for the covers in the carousel.
 *
 * A small cache holds the art for the selected cover and, when the
 * previous/next covers are shown, for up to two covers either side. Moving
 * one step reuses what is already loaded, so only one new picture has to be
 * decoded. Pictures are decoded one at a time, a moment after the selection
 * stops moving, so scrolling stays smooth.
 *
 * Memory: the selected cover's art is always loaded. Art for the side covers
 * is only loaded while enough memory is free, so on a 4MB console with a
 * custom background picture they fall back to placeholders.
 *
 * The selected box can be flipped over to show its back. The front is let go
 * before the back is loaded, so flipping costs no extra memory.
 */

#ifndef CAROUSEL_ART_H__
#define CAROUSEL_ART_H__

#include <stdbool.h>

#include "menu_state.h"

/**
 * Call once per frame while the carousel is shown.
 * @param side_covers True if the previous/next covers are shown.
 */
void carousel_art_update (menu_t *menu, bool side_covers);

/**
 * Draw the art of list entry `index` scaled to fit inside the given cover area.
 * @param alpha 255 for solid, lower to let the background show through.
 * @param center True for the selected cover (it can be mid-flip).
 * @return False if there is no art (yet), so the caller draws its placeholder.
 */
/**
 * The ring color for list entry `index`, taken from its cover (see
 * art_tint.h), once its art has arrived.
 * @return false if there is none (yet): use the accent color.
 */
bool carousel_art_tint (menu_t *menu, int index, color_t *out);

/** The same, telling "no color in this cover" from "its art is still on the way". */
typedef enum {
    ART_TINT_PENDING,   /**< the art has not arrived yet: the color is not known */
    ART_TINT_NONE,      /**< no art, or no strong color in it: use the accent color */
    ART_TINT_KNOWN,     /**< `out` holds the color */
} art_tint_state_t;
art_tint_state_t carousel_art_tint_state (menu_t *menu, int index, color_t *out);

/**
 * The shape of entry `index`'s art (width divided by height), once it has
 * arrived: Japanese boxes are tall, US and European ones wide. The cover is
 * drawn in that shape, so there are no bars at its sides.
 * @return false if there is no art (yet): use the usual shape.
 */
bool carousel_art_aspect (menu_t *menu, int index, float *aspect);

/**
 * How bright the art is drawn from now on: 255 as it is, lower is darker.
 * For the fade-in, which darkens covers this way instead of laying
 * see-through black over them. Set it back to 255 afterwards.
 */
void carousel_art_brightness (int bright);

bool carousel_art_draw (menu_t *menu, int index, int x0, int y0, int w, int h, int alpha, bool center);

/**
 * Draw a side cover's art turned away from the viewer (Angled Side Covers):
 * the box x0..x0+w, with its left edge `left` times the full height h and
 * its right edge `right` times it (1 = full height), centered on the box's
 * middle line. @return false if there is no art (yet).
 */
bool carousel_art_draw_angled (menu_t *menu, int index, int x0, int y0, int w, int h, float left, float right, int alpha);

/**
 * Draw the selected cover as a box with depth (3D Box), fitted inside the
 * given area: it opens out from a flat cover over a quarter of a second
 * once it has come to rest, with the top and sides from the box art when
 * they have arrived, plain color until then.
 * @return false if there is no art (yet), or the box is turning over: draw
 *         the flat cover instead.
 */
bool carousel_art_draw_box (menu_t *menu, int index, int x0, int y0, int w, int h);

/**
 * Turn the selected box over (front <-> back). Does nothing if no art is showing.
 * @return True if a flip started.
 */
bool carousel_art_flip (void);

/**
 * How wide the selected cover should be drawn right now: 1 normally,
 * shrinking to 0 and growing back while the box turns over.
 */
float carousel_art_flip_width (void);

/** Free all art, e.g. when leaving the browser. */
void carousel_art_reset (void);

#endif /* CAROUSEL_ART_H__ */
