/**
 * @file settings_menu.h
 * @brief Game-style settings screen: categories on the left, the selected
 *        category's settings in the middle, a description underneath.
 *
 * Replaces the stock settings editor screen (which stays in the source,
 * untouched, for easy merging with upstream).
 */

#ifndef SETTINGS_MENU_H__
#define SETTINGS_MENU_H__

#include "../menu_state.h"

void view_settings_menu_init (menu_t *menu);
void view_settings_menu_display (menu_t *menu, surface_t *display);

#endif /* SETTINGS_MENU_H__ */
