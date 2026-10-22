/**
 * @file box3d.h
 * @brief Draws a game's cover as a box with depth (3D Box).
 *
 * The box is a real little 3D box, turned a few degrees and seen slightly
 * from above, then flattened onto the screen with perspective: the front is
 * the cover, the top is the box art's top strip where there is one, and the
 * sides are the side pictures where there are any, otherwise plain color.
 * Only the faces turned toward the viewer are drawn, so the box can turn
 * either way. Nothing is stored: it is worked out each frame from the
 * pictures already loaded.
 */

#ifndef BOX3D_H__
#define BOX3D_H__

#include <libdragon.h>

typedef struct {
    surface_t *front;   /**< the cover (required) */
    surface_t *top;     /**< the top strip, or NULL for plain color */
    surface_t *left;    /**< the left side, or NULL for plain color */
    surface_t *right;   /**< the right side, or NULL for plain color */
    color_t side;       /**< the color of faces without a picture */
    int bright;         /**< 255 as it is, lower is darker (the fade-in) */
    bool back;          /**< the back of the box is showing: the side pictures change places (the top
                             and bottom labels stay as they are, reading the right way up) */
} box3d_art_t;

typedef struct {
    float yaw;          /**< turn in degrees: above 0 shows the left side, below 0 the right */
    float pitch;        /**< degrees seen from above: above 0 shows the top, below 0 the bottom */
    float amount;       /**< 0 the plain flat cover, 1 the full box; in between eases from one to the other */
    float fit_yaw;      /**< the angles the box's size is worked out at, so it keeps */
    float fit_pitch;    /**< one size while it turns (the resting angles) */
    float lift;         /**< pixels to move it down (negative: up); between pixels is fine */
} box3d_pose_t;

/** Draw the box fitted inside x0, y0, w, h (keeping its shape, centered), at the given pose. */
void box3d_draw (const box3d_art_t *art, int x0, int y0, int w, int h, const box3d_pose_t *pose);

#endif /* BOX3D_H__ */
