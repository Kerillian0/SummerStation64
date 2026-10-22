/**
 * @file box_motion.h
 * @brief How the 3D Box moves: the hover, and turning it by hand.
 *
 * Hover (option `box_hover`): the selected box floats up and down a few
 * pixels and sways gently from side to side, all timed by the clock.
 *
 * Inspect: while B is held on a row of covers, the stick or the D-Pad turns
 * the box, faster the further the stick is pushed. It can't be turned all
 * the way round (75 degrees either way, 40 below and 50 above), so it stays
 * readable. Scrolling waits meanwhile. Letting go of B brings the box back
 * to rest; up then turns it over as before, and B can be held again to look
 * at the back.
 */

#ifndef BOX_MOTION_H__
#define BOX_MOTION_H__

#include <stdbool.h>

#include "menu_state.h"

/** Which parts of the hover are on (option `box_hover`). */
typedef enum {
    BOX_HOVER_OFF,
    BOX_HOVER_FLOAT,        /**< up and down only */
    BOX_HOVER_SWAY,         /**< turning a little from side to side only */
    BOX_HOVER_BOTH,         /**< the default */
    BOX_HOVER_COUNT
} box_hover_t;

/** Name of a box_hover_t choice, for Settings. */
const char *box_motion_hover_name (int choice);

/**
 * Read B and the stick on a row of covers. While the box is being turned by
 * hand, this frame's moves are taken away from the row.
 * @return true while B is held over a 3D Box.
 */
bool box_motion_process (menu_t *menu);

/**
 * The box's pose right now: the resting angles plus the hover and any turn
 * by hand.
 * @param yaw, pitch In: the resting angles. Out: the angles to draw at.
 * @param lift       Out: pixels to move the box down (negative: up).
 */
void box_motion_pose (float *yaw, float *pitch, float *lift);

/**
 * Box Hover's float for a cover at the side, list entry `index` (each has
 * its own timing), in pixels to move it down (negative: up).
 * @param weight 1 at the side, down to 0 in the middle.
 */
float box_motion_side_lift (int index, float weight);

/** The box was drawn this frame (B only inspects a box that is showing). */
void box_motion_shown (void);

/** Another game was selected: forget any turn by hand at once. */
void box_motion_reset (void);

#endif /* BOX_MOTION_H__ */
