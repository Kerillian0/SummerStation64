/**
 * @file dd_pair.h
 * @brief 64DD pairing: a disk and the cartridge it is played with.
 *
 * Expansion disks run together with a cartridge: F-Zero X's Expansion Kit
 * with F-Zero X, and so on. A disk pairs by itself with the cartridge whose
 * game code ends in the same three letters as the disk's ID, for disks
 * whose ID starts with E or later (EFZJ with CFZJ; see docs/17_64dd.md),
 * plus Dezaemon 3D's prototype disk DEZA. A cartridge with a 'C' code
 * (made for the 64DD) is chosen over others, an original over a hack.
 *
 * The player can pair a disk with any cartridge, or with none, from its Z
 * menu ("Pair with a cartridge..."); that wins over the rule. Kept in
 * sd:/menu/ddpairs.txt: "ddpairs 1", then "disk <tab> cartridge" per line,
 * both from the top of the card, the cartridge "-" for none.
 *
 * A paired disk's info screen starts both with A (Z: the disk alone), and
 * the cover row's title panel says "With F-Zero X". Cartridges are looked
 * for in the Games list.
 */

#ifndef DD_PAIR_H__
#define DD_PAIR_H__

#include <stdbool.h>
#include "menu_state.h"

/**
 * The cartridge to play this disk with, or NULL for none.
 * @param disk_path Full path ("sd:/64DD/Game.ndd").
 * @param id The disk's ID (disk_info_t.id), noted for the cover row too.
 * @return A path the caller frees.
 */
path_t *dd_pair_cart (menu_t *menu, const char *disk_path, const char id[4]);

/**
 * The game an expansion disk needs its cartridge for ("F-Zero X"; "" when
 * not known), or NULL for a disk that plays alone. Every disk the code rule pairs is one: it
 * holds extra content for a cartridge game and won't start without it.
 */
const char *dd_pair_needs (const char id[4]);

/** "With F-Zero X" for the selected cover if it is a paired disk, else NULL. */
const char *dd_pair_with_text (menu_t *menu, const entry_t *disk);

/** After each Games frame: reads the selected disk's ID once it has rested. */
void dd_pair_after_draw (menu_t *menu, const entry_t *selected);

/** Z menu on a disk: choose its cartridge (full path of the disk). */
void dd_pair_pick (menu_t *menu, const char *disk_path);

/** While the choice is showing it has the controls: true if it took this frame's. */
bool dd_pair_process (menu_t *menu);

/** Draw the choice while it shows. */
void dd_pair_draw (void);

#endif /* DD_PAIR_H__ */
