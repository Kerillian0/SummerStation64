/**
 * @file prep.h
 * @brief "Prepare SD card": put each game's RetroAchievements set and badge
 * pictures on the card, for the menu's own (local) achievements. The card sits
 * in the PC's card reader; no N64 or USB cable is involved.
 *
 * Every N64 ROM on the card (.z64 / .n64 / .v64, any folder but menu/ and
 * saves/; not sc64menu.n64) is hashed the way RetroAchievements does (MD5 in z64 byte order),
 * looked up, and for each game RetroAchievements knows the card gets
 * (paths from the card's top):
 *
 *   menu/achievements/index.txt          "achindex 1", then per ROM:
 *                                        size <TAB> md5 <TAB> game id (0: unknown;
 *                                        1000000000 and up: a dump RetroAchievements
 *                                        knows but has no set for) <TAB> path
 *                                        ("N64(US)/Game.z64")
 *   menu/achievements/<id>/set.txt       "achset 1", "game <TAB> id <TAB> title",
 *                                        then per achievement: "a <TAB> id <TAB>
 *                                        points <TAB> type <TAB> badge <TAB> title
 *                                        <TAB> description" (the server's order;
 *                                        type: 0 plain, 1 missable, 2 progression,
 *                                        3 win)
 *   menu/achievements/<id>/logic.txt     "achlogic 1", then "id <TAB> definition"
 *   menu/achievements/<id>/icon.png      the game's icon
 *   menu/achievements/<id>/badges/<badge>.png, <badge>_lock.png
 *   menu/achievements/<id>/badges.pak    the same pictures in one file, which the
 *                                        menu reads: "BPK1", count, then offset
 *                                        and size per picture (big-endian; size 0:
 *                                        none), then the PNGs; 2i coloured and
 *                                        2i+1 locked for set.txt's achievement i,
 *                                        the last the icon
 *
 * Promoted achievements only, never the server's "Unknown Emulator" warning.
 * Nothing here is sent to the player's account: logging in only lets the
 * sets be downloaded. ROMs already in the index with the same size aren't
 * read again, and badges already on the card aren't downloaded again (the
 * pack is made again from them each time).
 */

#ifndef PREP_H__
#define PREP_H__

#include <stdbool.h>

/** Prepare the card whose top folder is `root` (e.g. "E:\"). The player must be logged in (ra_web_login). */
bool prep_card (const char *root);

#endif
