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
 * @param which IMAGE_BOXART_FRONT or IMAGE_BOXART_BACK.
 * @return NULL if there is none baked in (or no memory for it).
 */
component_boxart_t *baked_art_load (const char *game_code, file_image_type_t which);

/**
 * Free a cover if it came from baked_art_load().
 * @return False if it did not: free it the usual way.
 */
bool baked_art_free (component_boxart_t *art);

/**
 * A game's picture from wherever it can be had: baked art first, then the
 * SD card (ui_components_boxart_init). Free it with baked_art_release().
 */
component_boxart_t *baked_art_open (const char *storage_prefix, const char *game_code, const char *title, file_image_type_t which);

/** Free a picture from baked_art_open(), whichever kind it is. NULL is fine. */
void baked_art_release (component_boxart_t *art);

#endif /* BAKED_ART_H__ */
