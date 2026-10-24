/**
 * @file art_report.h
 * @brief Missing art report: which games lack a cover, a top label, a
 *        cartridge picture, a description or details.
 *
 * Started from Settings > Library. It goes through every game on the
 * Games tab, one a frame, looking where the covers and Game info look:
 * art baked into the menu file first, then the SD card's metadata folder
 * (homebrew's by its title). Games that lack something are written to
 * sd:/menu/missingart.txt, one per line, for the player or the Companion
 * to fill in. 64DD disks are not checked.
 *
 * Line format, tab-separated: the game's art folder under menu/metadata
 * ("N/Z/S/E", or "homebrew/<title>"), what kind of game it is (game,
 * hack, homebrew), what it lacks (front, top, cartridge, description,
 * date, developer, comma-separated), and the game's file from the top of
 * the card.
 */

#ifndef ART_REPORT_H__
#define ART_REPORT_H__

#include <stdbool.h>
#include "menu_state.h"

/** Start a report (lists the games; the checking is done by art_report_step()). */
void art_report_start (menu_t *menu);

/** Check the next game, and write the file after the last. Call once a frame. */
void art_report_step (menu_t *menu);

/** Stop a report under way; nothing is written. */
void art_report_cancel (void);

/** True while a report is under way. */
bool art_report_busy (void);

/** What to show in Settings: "Not made yet", "Checking 12 of 99", "12 of 99 games". */
const char *art_report_status (void);

#endif /* ART_REPORT_H__ */
