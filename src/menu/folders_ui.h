/**
 * @file folders_ui.h
 * @brief The plain file list on the Folders tab.
 *
 * Every file and folder by its real name, one per line, with its size.
 * This is where pictures, music, text files and zips are opened, and where
 * the start folder (the one the Games tab shows) is chosen.
 */

#ifndef FOLDERS_UI_H__
#define FOLDERS_UI_H__

#include "menu_state.h"

/** Draw the current folder's path and its list, under the tab bar. */
void folders_ui_draw (menu_t *menu);

#endif /* FOLDERS_UI_H__ */
