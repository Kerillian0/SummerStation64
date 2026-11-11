/**
 * @file pad_test.h
 * @brief Controller test: Settings > Console > Tools > Controller Test.
 *
 * An N64 controller drawn on screen (shapes only, no picture) whose buttons
 * light up as they are pressed, the stick a knob moving in its gate. Beside
 * it, the stick's range: how far it has reached in each of eight directions,
 * drawn against a new stick's reach, as a percentage and a word (Like new,
 * Worn, Very worn). The four ports along the top light when a controller is
 * in; the one shown is whichever was last used. Its accessory is named
 * (Rumble Pak, Controller Pak, Transfer Pak), and with a Rumble Pak, Z + A
 * rumbles it for half a second.
 *
 * Every button is tested, so none of them leaves: holding B for two seconds
 * does, while a ring fills. Buttons are read straight from the controllers.
 *
 * A page of Settings, as the theme editor is.
 */

#ifndef PAD_TEST_H__
#define PAD_TEST_H__

#include <stdbool.h>

#include "menu_state.h"

/** Open the test. */
void pad_test_open (void);

/** True while it is open. */
bool pad_test_active (void);

/** Read the controllers. */
void pad_test_process (menu_t *menu);

/** Draw it over the background. */
void pad_test_draw (void);

#endif /* PAD_TEST_H__ */
