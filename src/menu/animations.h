/**
 * @file animations.h
 * @brief The Animations setting: Snappy (the menu as it always was) or Showy
 * (extra movement: art fading in, covers growing into place, names sliding
 * in). Showy's animations never hold the player up: any button press or
 * move finishes the ones already running.
 */

#ifndef ANIMATIONS_H__
#define ANIMATIONS_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ANIMATIONS_SNAPPY,
    ANIMATIONS_SHOWY,
    ANIMATIONS_COUNT
} animations_t;

/** The name of a choice, for Settings. */
const char *animations_name (int choice);

/** Is Showy chosen? */
bool animations_showy (void);

/** A button was pressed or a direction moved (from actions.c): animations
 *  started before now are finished at once. */
void animations_note_press (void);

/** How far an animation started at `since` and lasting `ms` has come, from
 *  0 to 1. Always 1 on Snappy, and once a press has come after `since`. */
float animations_progress (uint64_t since, int ms);

/** Easing: fast at first, settling into place. */
float animations_ease_out (float t);

/** Easing that runs a little past the end and comes back (0 to about 1.1 to 1). */
float animations_ease_back (float t);

/** When a button was last pressed or a direction moved (get_ticks_ms()). */
uint64_t animations_last_press_ms (void);

/** Whether a pop-up was up last frame. */
bool animations_popup_up (void);

/** The way the selection last moved on a row of covers: 1 next, -1 previous. */
void animations_note_direction (int direction);
int animations_direction (void);

/** A highlight bar that glides to its new place instead of jumping. */
typedef struct {
    int from, to;           /* where it glides from and to, in pixels */
    uint64_t since;         /* when it set off */
    uint64_t last_ms;       /* when it was last drawn: one not drawn for a while starts in place */
    bool set;
} animations_glide_t;

/** Where to draw a highlight whose row is at `target` now (pixels, kept even). */
int animations_glide (animations_glide_t *glide, int target);

/** The same, taking `ms` to glide, and starting in place when it hasn't
 *  been drawn for `fresh_ms` (longer for one that crosses screens). */
int animations_glide_timed (animations_glide_t *glide, int target, int ms, int fresh_ms);

/** From ui_components_dialog_draw(): a pop-up drawn for the first time
 *  grows open from its middle. animations_dialog() changes the box to draw;
 *  animations_dialog_clip(), once the box is drawn, keeps what is drawn
 *  inside it to the box while it grows, until animations_dialog_end(). */
void animations_dialog (int *x0, int *y0, int *x1, int *y1);
void animations_dialog_clip (void);
void animations_dialog_end (void);

/** At the end of each frame's drawing (before the Performance Overlay): a
 *  pop-up that has just closed on this screen shrinks away. */
void animations_draw_late (int screen);

/** Once a frame, after the screen has been drawn (menu.c). */
void animations_frame_done (int screen);

/** Game info's cover flies in from the row of covers, and back to it with B.
 *  The screen showing the game's cover notes where it drew it, every frame;
 *  the other screen, when it first shows the cover (a row and Game info, in
 *  either direction, within a moment), takes that place to fly from. */
void animations_cover_note (int screen, int x0, int y0, int x1, int y1);
bool animations_cover_take (int screen, int from[4]);

#endif
