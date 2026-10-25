/**
 * @file clip.h
 * @brief Recording a short clip of the menu, for an animated GIF.
 *
 * Settings > System > Record Clip arms it. Once Settings is left, three
 * ticks count down and the clip starts; it stops by itself after the
 * chosen length (option `clip_length`). Each drawn frame is saved as
 * sd:/menu/clips/clipNNNN/frameNNNN.png (size and dither: options
 * `clip_size`, `clip_dither`), and clip.txt says how many frames at what
 * rate (`clip_fps`). The Companion turns the folder into a GIF.
 *
 * Saving a frame takes far longer than a frame lasts, so while recording
 * the menu's clock (get_ticks_ms(), swapped in by the linker: --wrap) moves
 * exactly one frame's time per frame saved. Everything timed by it (slides,
 * hover, the fade) plays back at its real speed in the clip, while the
 * console itself runs slowly. Afterwards the clock carries on from where
 * the clip left it, so nothing jumps.
 */

#ifndef CLIP_H__
#define CLIP_H__

#include <stdbool.h>
#include <libdragon.h>
#include "menu_state.h"

typedef enum { CLIP_FPS_15, CLIP_FPS_30, CLIP_FPS_COUNT } clip_fps_t;
typedef enum { CLIP_LENGTH_5, CLIP_LENGTH_10, CLIP_LENGTH_15, CLIP_LENGTH_COUNT } clip_length_t;

/** Settings' Record Clip: start once Settings is left. */
void clip_arm (menu_t *menu);

/** Once a frame has been drawn (after screenshot_after_frame()): count down, or save the frame. */
void clip_after_frame (menu_t *menu, surface_t *display);

/** True while a clip is being recorded (or counted down to). */
bool clip_busy (void);

/** What Settings shows beside Record Clip: "Ready", "Recording 40 of 150", "Saved clip0003". */
const char *clip_status (menu_t *menu);

/** Names for Settings. */
const char *clip_fps_name (int value);
const char *clip_length_name (int value);

#endif /* CLIP_H__ */
