/**
 * @file intro_logo.h
 * @brief The spinning 3D "N" shown in the intro.
 *
 * Four pillars joined by four slanted bars, so that each side reads as an
 * N, in the vaporwave colors of the picture the project owner supplied as
 * reference, and polished-looking like the one that spins at the start of
 * Ocarina of Time (the owner's second reference): each face is shaded from
 * top to bottom. The pillars carry the reference's two patterns, a marbled
 * "hologram" and pink-to-blue stripes. It is worked out and drawn as
 * triangles each frame; the two small patterns (4 KB) are the only memory
 * it uses, and only while the intro plays.
 *
 * The shape is Nintendo's N64 logo. The owner chose to use it (2026-10-08)
 * and will take it out if asked to.
 */

#ifndef INTRO_LOGO_H__
#define INTRO_LOGO_H__

/** The two looks (option `intro_logo` in options.ini). */
typedef enum {
    INTRO_LOGO_VAPORWAVE,   /**< patterned sides, after the owner's reference picture (the default) */
    INTRO_LOGO_CLASSIC,     /**< the console's own green, blue, red and yellow */
    INTRO_LOGO_COUNT
} intro_logo_t;

/** Name of a look, for the settings screen. */
const char *intro_logo_name (int choice);

/** Choose the look and load what it needs (the vaporwave patterns, 4 KB). Call when the intro starts. */
void intro_logo_open (intro_logo_t which);

/** Free them again. Call when the intro ends. */
void intro_logo_close (void);

/**
 * Draw the logo.
 * @param center_x,center_y Where its middle goes on the screen.
 * @param size How many pixels half its width takes.
 * @param angle How far it is turned, in degrees.
 */
void intro_logo_draw (int center_x, int center_y, float size, float angle);

#endif /* INTRO_LOGO_H__ */
