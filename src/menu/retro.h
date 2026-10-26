/**
 * @file retro.h
 * @brief The Retro tab: games for the menu's emulators (NES, SNES, Game Boy,
 *        Game Boy Color, and the others the stock menu can start).
 *
 * The Retro tab is the Games list on a shelf of its own (games_filter.c);
 * C-up / C-down step through All, NES, SNES, Game Boy and Game Boy Color.
 * Emulator games stay off the Games tab. Each system's cover has its own
 * box shape until its picture arrives, and the picture comes from
 * sd:/menu/metadata/<system>/<file name without extension>/boxart_front.png
 * (and boxart_back.png, boxart_spine.png for the 3D box's sides and
 * boxart_top.png for its top and bottom), the
 * system being nes, snes, gb or gbc.
 */

#ifndef RETRO_H__
#define RETRO_H__

#include <stdbool.h>
#include "ui_components.h"

typedef enum {
    RETRO_OTHER,        /**< another emulator's game (Sega, MSX, ...): under All only */
    RETRO_NES,
    RETRO_SNES,
    RETRO_GB,
    RETRO_GBC,
    RETRO_SYSTEMS
} retro_system_t;

/** Which system a file is for, by its extension. */
retro_system_t retro_system (const char *name);

/** "NES", "SNES", "Game Boy", "Game Boy Color" ("Other" for the rest). */
const char *retro_name (retro_system_t system);

/** Short, for a cover with no picture: "NES", "SNES", "GB", "GBC", "EMU". */
const char *retro_short_name (retro_system_t system);

/** "NES game", ... for the line under the title. */
const char *retro_kind (retro_system_t system);

/** The shape of the system's box, width over height. */
float retro_aspect (retro_system_t system);

/** The system's art folder under menu/metadata ("nes"), or NULL for systems without one. */
const char *retro_folder (retro_system_t system);

/**
 * Start decoding a picture of an emulator game (`name` from the top of the
 * card, or a full path). Uses the PNG decoder, like the stock box art.
 * @return NULL if there is no such picture.
 */
component_boxart_t *retro_art_load (const char *storage_prefix, const char *name, file_image_type_t which);

#endif /* RETRO_H__ */
