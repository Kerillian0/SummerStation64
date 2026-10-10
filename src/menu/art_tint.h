/**
 * @file art_tint.h
 * @brief Colors the ring round a cover to match the cover.
 *
 * The ring takes the strongest color in the picture: the shade that covers
 * the most of it, ignoring greys, near-blacks and the red strip down the
 * right-hand side of American N64 boxes (which would make every ring red).
 * Option `ring_tint`: off, on the Game info screen only, or there and on
 * the cover rows too.
 */

#ifndef ART_TINT_H__
#define ART_TINT_H__

#include <stdbool.h>
#include <libdragon.h>

/** Where the ring is tinted (option `ring_tint` in options.ini). */
typedef enum {
    RING_TINT_OFF,          /**< always the theme's accent color */
    RING_TINT_GAME_INFO,    /**< on the Game info screen only (the default) */
    RING_TINT_EVERYWHERE,   /**< on the cover rows as well */
    RING_TINT_COUNT
} ring_tint_t;

/** Name of a choice, for the settings screen. */
const char *art_tint_name (int choice);

/** Whether the ring should be tinted on the Game info screen / the cover rows. */
bool art_tint_on_game_info (void);
bool art_tint_on_covers (void);

/**
 * Work out a picture's ring color. Reads every second pixel each way, so it
 * is quick enough to do once when a cover arrives.
 * @return false if the picture has no strong color (keep the accent color),
 *         or is not in the 16-bit format the covers use.
 */
bool art_tint_from (const surface_t *image, color_t *out);

#endif /* ART_TINT_H__ */
