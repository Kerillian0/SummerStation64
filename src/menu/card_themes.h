/**
 * @file card_themes.h
 * @brief Themes kept on the SD card, one folder each.
 *
 * sd:/menu/themes/<name>/theme.ini (or theme.txt) is a theme; the folder's
 * name is what the player sees. Up to 32 are listed, A-Z, after the
 * built-in themes in Settings > Display > Theme. Later a theme's folder can
 * also hold its font and sounds. The old single sd:/menu/theme/theme.ini
 * is still the "From SD Card" choice.
 *
 * A card theme is option `theme` = THEME_CHOICE_CARD + its place in the
 * list, and option `theme_key` remembers its folder name (hashed), so a
 * theme added or removed on the PC doesn't swap the one in use for another.
 */

#ifndef CARD_THEMES_H__
#define CARD_THEMES_H__

#include <stdbool.h>

#include "builtin_themes.h"

#define CARD_THEMES_DIR     "sd:/menu/themes"
#define CARD_THEMES_MAX     (32)
#define THEME_CHOICE_CARD   (BUILTIN_THEME_COUNT)                       /**< the first card theme's option value */
#define THEME_CHOICE_COUNT  (BUILTIN_THEME_COUNT + CARD_THEMES_MAX)     /**< values for the Settings list */

/** Read the themes folder again (when Settings opens, so new folders show). */
void card_themes_rescan (void);

/** The name of a choice for Settings: a built-in theme, a card theme's folder, or "" if none. */
const char *card_themes_choice_name (int choice);

/** Note which card theme was chosen (call after option `theme` changes). */
void card_themes_note_choice (void);

/** The folder of the card theme in use (sd:/menu/themes/<name>), or NULL if the choice is not a card theme or its folder is gone. */
const char *card_themes_folder (void);

/** The theme file of the card theme in use, or NULL if the choice is not a card theme or its folder is gone. */
const char *card_themes_path (void);

#endif /* CARD_THEMES_H__ */
