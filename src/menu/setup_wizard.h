/**
 * @file setup_wizard.h
 * @brief The setup wizard: Vee asks a few questions the first time the menu
 * starts on a card, and again whenever Settings > Console > About > Setup
 * Wizard is chosen.
 *
 * One question a page, Vee large on the left with the question in her
 * speech panel and what the highlighted answer does under it: the console
 * and TV (kept as option `console_type`), Video Output (240p suggested),
 * Preferred Region, Hide Region Tags, Region Copies, the theme (shown as it
 * is highlighted), Animations, the intro and fade-in, how often Vee talks,
 * her mood and her name. Each page starts on the present value. A answers
 * and goes on, B goes back a page, START keeps the rest as it is and
 * finishes. What is chosen applies at once and is saved at the end.
 *
 * The first time, it runs on the start-up screen after the intro (in place
 * of the credits screen the stock menu shows), and the first tab fades in
 * after it; the card counts as set up only once it is finished, so a power
 * cut halfway asks again.
 */

#ifndef SETUP_WIZARD_H__
#define SETUP_WIZARD_H__

#include <stdbool.h>
#include "menu_state.h"

/** Option `console_type`: what the menu is played on (asked by the wizard). */
typedef enum {
    CONSOLE_TYPE_CRT,           /**< an N64 on a CRT TV (the default) */
    CONSOLE_TYPE_HDMI_MOD,      /**< an N64 with an HDMI mod or an upscaler */
    CONSOLE_TYPE_ANALOGUE_3D,
    CONSOLE_TYPE_MODRETRO_M64,
    CONSOLE_TYPE_COUNT
} console_type_t;

/** On the start-up screen (startup.c): run the wizard after the intro if the card hasn't been set up. */
void setup_wizard_begin_first_run (menu_t *menu);

/** The start-up screen's frame while the wizard runs there. @return false when it isn't running there. */
bool setup_wizard_display (menu_t *menu, surface_t *display);

/** From Settings: run it again (Settings hands it its buttons and frames while it is active). */
void setup_wizard_open (menu_t *menu);

/** True while it runs inside Settings. */
bool setup_wizard_active (void);

/** Settings' hooks: buttons, and the page (the background already drawn). */
void setup_wizard_process (menu_t *menu);
void setup_wizard_draw (void);

#endif /* SETUP_WIZARD_H__ */
