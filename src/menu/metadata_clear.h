/**
 * @file metadata_clear.h
 * @brief Game info > Options > "Clear Metadata...": take away a game's own
 *        pictures and details from the SD card, so they can be added again.
 *
 * Three choices: pictures and details, pictures only, details only. The
 * files the game uses on the card (its metadata folder's pictures,
 * metadata.ini and description.txt; for homebrew, its title folder's
 * pictures and the Game.metadata.ini / Game.description.txt beside it) are
 * counted, the player is asked, and they are moved, not deleted, to
 * sd:/menu/trash/<date-time>/ with their folders kept. The title logo and
 * preview clip stay. Covers and details built into the menu (art.pak, the
 * menu file) can't be cleared and keep showing.
 */

#ifndef METADATA_CLEAR_H__
#define METADATA_CLEAR_H__

#include <stdbool.h>
#include "menu_state.h"
#include "ui_components.h"

/** The list "Clear Metadata..." opens: Pictures and details / Pictures only / Details only. */
component_context_menu_t *metadata_clear_menu (void);

/** While its question or its answer is up: handles A and B and returns true. */
bool metadata_clear_prompt_process (menu_t *menu);

/** Draws the question or the answer, if one is up. */
void metadata_clear_prompt_draw (void);

#endif /* METADATA_CLEAR_H__ */
