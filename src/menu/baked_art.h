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

/**
 * The same, for the cover rows: without the Expansion Pak a front or a
 * cartridge comes in its 256-colour copy when the art file has one (half the memory; its colour
 * table from baked_art_palette(), loaded for drawing by baked_art_tlut()).
 */
component_boxart_t *baked_art_load_row (const char *game_code, file_image_type_t which);

/** The colour table of a 256-colour picture from baked_art_load_row(), else NULL. */
const uint16_t *baked_art_palette (const surface_t *image);

/** Before drawing a cover picture: load its colour table if it has one (and
 *  switch the chip to look colours up), else switch looking up off. Call after
 *  the drawing mode is set. */
void baked_art_tlut (const surface_t *image);

/** True if this picture of the game is baked in (nothing is loaded). */
bool baked_art_has (const char *game_code, file_image_type_t which);

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

/** @brief Whether the box art file (sd:/menu/art.pak) is in use. */
typedef enum {
    BAKED_PACK_NOT_USED,    /**< not a SummerCart64: the card's pictures are used */
    BAKED_PACK_OK,          /**< read and in use */
    BAKED_PACK_MISSING,     /**< not on the card */
    BAKED_PACK_BAD,         /**< there, but not one this menu reads (another version, damaged) */
} baked_pack_status_t;

/** @brief Open the art file if not tried yet, and say how it went. */
baked_pack_status_t baked_art_pack_status (void);

#endif /* BAKED_ART_H__ */
