/**
 * @file cover_clip.h
 * @brief A game's preview clip in the selected cover's place on the cover
 * rows (Games and its shelves, inside a collection, Recent, Favorites), as
 * Game info shows it: once the cover has rested 2 seconds, the clip fades
 * in over it; any move or button stops it and the cover is back at once.
 *
 * The clip itself is played by video_preview.c (Expansion Pak, Video
 * Previews on). Option `cover_clips` (Settings > Look > Game Info > Clips on
 * Covers) switches it off for the rows only. The clip is looked for once
 * per rest, after the frame is drawn (one card read); hacks without a code
 * of their own get none, as on Game info.
 */

#ifndef COVER_CLIP_H__
#define COVER_CLIP_H__

#include <stdbool.h>
#include "menu_state.h"

/** After the frame (from carousel_art_update()): notice moves, look for the clip once rested. */
void cover_clip_after_draw (menu_t *menu);

/** The covers were left: stop the clip and free it. */
void cover_clip_reset (void);

/**
 * Keep the clip for list entry `index` going (call each frame its cover is
 * at rest in the middle). @return The cover's remaining opacity over the
 * clip: 1 while there is no picture yet, down to 0.
 */
float cover_clip_frame (menu_t *menu, int index);

/**
 * Draw the clip for list entry `index` flat in the cover's box, if it is ready.
 * @param shown_w, shown_h  Out: the size it was drawn at (for the ring).
 * @return The cover's remaining opacity over it (1 = the cover still fully
 *         shows, so nothing was drawn; 0 = the clip alone).
 */
float cover_clip_draw (menu_t *menu, int index, int x0, int y0, int w, int h, int *shown_w, int *shown_h);

/** Name of an option `cover_clips` value, for Settings. */
const char *cover_clip_name (int choice);

#endif
