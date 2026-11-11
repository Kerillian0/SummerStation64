/**
 * @file cover_deal.h
 * @brief The cover row deals itself in when the menu starts (after the
 * intro, or straight away without one): the side covers slide in from
 * their own sides, the outermost first, fading in as they come, and the
 * selected game's cover drops into the middle last with a little bounce;
 * the ring appears round it once it has landed.
 *
 * Once per start (power-on or RESET), on the first cover row shown, with
 * Cover Slide on, once the covers' art has loaded (loading makes slow
 * frames, which would make the deal skip). Any press finishes it at once. Drawn by carousel_draw()
 * in views/browser.c, which asks where each cover is.
 */

#ifndef COVER_DEAL_H__
#define COVER_DEAL_H__

#include <stdbool.h>

/** Start of the cover row's drawing: begins the deal the first time (it
 *  waits for the covers' art: `middle_art` the selected one has its art,
 *  `with_art` how many of the row's covers have theirs; and for the start-up
 *  fade's picture to be up, `faded_in`), notices its end. */
void cover_deal_frame (bool middle_art, int with_art, bool faded_in);

/** The covers are being dealt in. */
bool cover_deal_running (void);

/** A deal is still to come or under way (the start-up fade keeps its picture short for it). */
bool cover_deal_ahead (void);

/**
 * Where the cover `offset` places from the middle is in the deal.
 * @param dx, dy   Out: pixels from its resting place.
 * @param scale    Out: its size against its resting size.
 * @param alpha    Out: how far faded in, 0 to 1.
 * @return false while it hasn't been dealt yet (not drawn).
 */
bool cover_deal_place (int offset, int *dx, int *dy, float *scale, float *alpha);

/** The selected cover hasn't landed yet: no ring. */
bool cover_deal_hides_ring (void);

#endif
