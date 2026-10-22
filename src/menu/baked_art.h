/**
 * @file baked_art.h
 * @brief Box art carried inside the menu's own file.
 *
 * Covers baked in at build time (see assets/boxart/README.md) are ready to
 * use: no searching the SD card for the picture and no PNG to unpack. A
 * game without a baked cover gets NULL here, and the caller reads its art
 * from the card as before.
 */

#ifndef BAKED_ART_H__
#define BAKED_ART_H__

#include <stdbool.h>
#include "ui_components.h"

/**
 * The baked cover for a game, loaded and ready to draw.
 * @param game_code The four-character code from the ROM's header.
 * @param which IMAGE_BOXART_FRONT, _BACK, _TOP, _LEFT, _RIGHT or IMAGE_GAMEPAK_FRONT.
 * @return NULL if there is none baked in (or no memory for it).
 */
component_boxart_t *baked_art_load (const char *game_code, file_image_type_t which);

/** True if a picture came from baked_art_load(). */
bool baked_art_owns (component_boxart_t *art);

/**
 * Free a cover if it came from baked_art_load().
 * @return False if it did not: free it the usual way.
 */
bool baked_art_free (component_boxart_t *art);

/**
 * False if the SD card's box art pack has no cover of this game's own: no
 * folder for its region, and the folder without the region letter sits
 * beside folders for single regions, so its cover is another region's
 * (in the pack, the European one: Dezaemon 3D, NDZJ, would get Duke Nukem:
 * Zero Hour's). European games may use it. Reads the card only when the
 * game has no folder of its own.
 */
bool baked_art_card_ok (const char *storage_prefix, const char *game_code);

/**
 * A game's picture from wherever it can be had: baked art first, then the
 * SD card (ui_components_boxart_init). Free it with baked_art_release().
 */
component_boxart_t *baked_art_open (const char *storage_prefix, const char *game_code, const char *title, file_image_type_t which);

/** Free a picture from baked_art_open(), whichever kind it is. NULL is fine. */
void baked_art_release (component_boxart_t *art);

#endif /* BAKED_ART_H__ */
