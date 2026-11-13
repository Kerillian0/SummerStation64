/**
 * @file whats_new.h
 * @brief What's new: Vee shows what came with the menu's new version, once
 * after the menu file is updated and from Settings > Console > About >
 * What's New.
 *
 * The list is rom:/whatsnew.txt (assets/data/whatsnew.txt, checked on the PC
 * by tests/whats_new_check.py): every version newer than the one this
 * console last showed, newest first. Vee on the left, her panel on top says
 * what the highlighted item is; A "Show me" opens Settings on the item's
 * row (B there comes back to the list). Read when it opens, freed when it
 * closes. Which version was last shown is option `whats_new_seen`, the
 * same for every player; a brand-new card (the setup wizard's first start)
 * counts as having seen it.
 */

#ifndef WHATS_NEW_H__
#define WHATS_NEW_H__

#include <stdbool.h>
#include "menu_state.h"

/** On the start-up screen (startup.c), before "Who's playing?": show the list after the intro if there is something new. */
void whats_new_begin_startup (menu_t *menu);

/** The start-up screen's frame while the list runs there. @return false when it isn't running there. */
bool whats_new_display (menu_t *menu, surface_t *display);

/** From Settings: show this version's list (and any newer ones not yet seen). */
void whats_new_open (menu_t *menu);

/** True while the list shows inside Settings. */
bool whats_new_active (void);

/** Settings' hooks: buttons, and the page (the background already drawn). */
void whats_new_process (menu_t *menu);
void whats_new_draw (void);

/** A "Show me" asked for: the Settings path to go to ("Look > Theme > Animations"), once; NULL if none. */
const char *whats_new_take_jump (void);

/** Back from the row "Show me" went to: the list shows again. */
void whats_new_resume (void);

#endif /* WHATS_NEW_H__ */
