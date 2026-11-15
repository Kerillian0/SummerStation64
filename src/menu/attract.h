/**
 * @file attract.h
 * @brief Attract mode: left alone on a row of covers (Games and its shelves,
 * a collection, Recent, Favorites) for the time chosen in Settings > Console
 * > TV & Video > Attract Mode, the row drifts on its own at a steady pace,
 * never stopping, in the direction chosen (Attract Direction). The game
 * passing through the middle is the selected one: its clip plays on its
 * cover as it goes (Expansion Pak) and Vee has her word about it. The tab
 * bar, clock, title panel and hints are dimmed meanwhile.
 *
 * The first press only wakes it: the row eases into place on the game
 * nearest the middle and nothing else happens. Off by default.
 */

#ifndef ATTRACT_H__
#define ATTRACT_H__

#include <stdbool.h>
#include "menu_state.h"

/** Values of option `attract_mode`: how long the menu waits first. */
typedef enum {
    ATTRACT_OFF,
    ATTRACT_2_MIN,
    ATTRACT_5_MIN,
    ATTRACT_10_MIN,
    ATTRACT_COUNT
} attract_wait_t;

/** Values of option `attract_direction`. */
typedef enum {
    ATTRACT_RIGHT_TO_LEFT,
    ATTRACT_LEFT_TO_RIGHT,
    ATTRACT_DIRECTION_COUNT
} attract_direction_t;

/**
 * Call once a frame from a cover row's input handling, after its own menus
 * and pop-ups have had their turn (none is open when this is reached).
 * Starts attract mode once the menu has been left alone long enough.
 * @param entries  How many games the row holds.
 * @return How far the selection moves this frame: -1, 0 or +1 (silently,
 *         wrapping round the row).
 */
int attract_frame (int entries);

/** Attract mode is moving the covers. */
bool attract_running (void);

/** The covers' offset from their resting places, in cover positions, while
 *  attract mode drifts them or they ease back into place afterwards; 0 otherwise. */
float attract_slide (void);

/** From actions_update(): a press while attract mode runs only wakes it.
 *  @return true if this frame's press was taken for that (clear the actions). */
bool attract_wake (void);

/** Dim the tab bar, clock, title panel and hints while attract mode runs
 *  (call before the assistant is drawn, so Vee's words stay bright). */
void attract_draw_dim (void);

/** Parts of a frame timed while attract mode runs (the log's `attract:` lines). */
typedef enum {
    ATTRACT_TIME_COVERS,    /**< drawing the cover row */
    ATTRACT_TIME_CLIP,      /**< ... of which the moving clip */
    ATTRACT_TIME_DIM,       /**< fading the tab bar, panel and hints */
    ATTRACT_TIME_DECODE,    /**< the clip's own update (video_preview_stats()) */
    ATTRACT_TIME_COUNT
} attract_time_t;

/** Add `us` microseconds to a timed part (nothing unless attract mode runs). */
void attract_time (attract_time_t part, uint32_t us);

/** Once a frame from the row's input handling: count it, and log the timing every 2 seconds. */
void attract_time_frame (void);

/** Name of an option `attract_mode` value, for Settings. */
const char *attract_wait_name (int value);

/** Name of an option `attract_direction` value, for Settings. */
const char *attract_direction_name (int value);

#endif
