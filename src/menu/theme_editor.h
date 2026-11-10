/**
 * @file theme_editor.h
 * @brief Make a theme on the console: Settings > Display > Edit Theme.
 *
 * Starts from the theme in use. The player changes its colors, background
 * and pattern, and the whole menu shows each change at once, with a small
 * sample of the Games screen beside the list. A color is picked from a grid
 * of ready colors or with Hue / Color / Light sliders, both on one panel.
 * "Save as Theme" names it with the letter dial and writes
 * sd:/menu/themes/<name>/theme.ini, the same file the Companion's Theme
 * Maker makes, and switches to it. Saving a card theme under another name
 * asks whether its music, sounds and font come along, and copies them a
 * piece each frame. New Theme starts from a look the player picks instead.
 *
 * It is a page of Settings, not a screen of its own: Settings hands it the
 * buttons and the drawing while it is open.
 */

#ifndef THEME_EDITOR_H__
#define THEME_EDITOR_H__

#include <stdbool.h>

#include "menu_state.h"

/** Open the editor on the theme in use. */
void theme_editor_open (void);

/** Make a new theme: first pick the look it starts from (a built-in one or plain dark), then edit it as usual. */
void theme_editor_new (void);

/** True while the editor is open. */
bool theme_editor_active (void);

/** Read this frame's buttons. */
void theme_editor_process (menu_t *menu);

/** Draw the editor over the background. */
void theme_editor_draw (void);

#endif /* THEME_EDITOR_H__ */
