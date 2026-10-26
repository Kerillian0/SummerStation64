/**
 * @file game_info_more.h
 * @brief Game info for 64DD disks and emulator games, laid out like an N64
 *        cartridge's (game_info_ui_draw_page()).
 *
 * Opened by the stock disk and emulator screens (load_disk.c,
 * load_emulator.c), which keep their buttons and loading; this holds the
 * pictures and details and draws the page. Details come from a
 * metadata.ini beside the pictures (name, author, release-date, genre,
 * num-players, short-desc), and the full text from description.txt there:
 * - a disk: by its ID, baked into the menu file or under
 *   sd:/menu/metadata/D/M/P/J/ (or without the last letter), as N64 games;
 * - an emulator game: sd:/menu/metadata/<nes|snes|gb|gbc>/<file name>/ (retro.h).
 */

#ifndef GAME_INFO_MORE_H__
#define GAME_INFO_MORE_H__

#include <stdbool.h>
#include "menu_state.h"

/** A disk with this ID, at this full path. */
void game_info_more_open_disk (menu_t *menu, const char *path, const char id[4]);

/** An emulator game at this full path. */
void game_info_more_open_retro (menu_t *menu, const char *path);

/** Let go of the pictures and text (leaving the screen). */
void game_info_more_close (void);

/** ◀▶: the front or the back of the box. */
void game_info_more_pictures (menu_t *menu);

/** START: the details pop-up on or off; true while it shows. */
void game_info_more_details (bool show);
bool game_info_more_details_showing (void);

/**
 * Draw a disk's page and, if asked, its details.
 * @param cart The cartridge it is played with (file name), or NULL.
 * @param needs The game an expansion disk needs (dd_pair_needs()), or NULL.
 * @param alone The player chose to play it with no cartridge (dd_pair_alone()).
 */
void game_info_more_draw_disk (menu_t *menu, const disk_info_t *disk, int swap_disks, const char *cart, const char *needs, bool alone);

/** Draw an emulator game's page and, if asked, its details. */
void game_info_more_draw_retro (menu_t *menu, const char *system_name);

/** True if the backdrop covers the screen (the shared background need not be drawn). */
bool game_info_more_backdrop_ready (void);

#endif /* GAME_INFO_MORE_H__ */
