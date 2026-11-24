/**
 * @file tab_names.h
 * @brief Names of the player's own for the places on the tab bar, and the
 *        folder a folder tab shows.
 *
 * Settings > Games > Tabs: each place (First Tab ... Eighth Tab) can be
 * given a name, typed with the naming keyboard; the tab shows it instead of
 * its usual one ("Utilities" for a folder of tools, "Zelda" for a
 * collection). The name belongs to the place, so it stays when the place is
 * set to show something else. An empty name gives the tab back its usual
 * one. A place set to show a folder (Folders tab, Z: "Make ... a tab")
 * keeps that folder here too. Kept per player in menu/tabnames.txt
 * (`tabnames 1`, then `<place 1-8> <name>` and `f<place 1-8> <folder>`
 * lines; a folder as "/Utilities", from the top of the card).
 */

#ifndef TAB_NAMES_H__
#define TAB_NAMES_H__

/** The longest name a tab can have (a tab shows about 10 letters; longer ones end in ...). */
#define TAB_NAME_MAX    (16)

/** The longest folder path a folder tab keeps. */
#define TAB_FOLDER_MAX  (127)

/** The name given to a place (0 to 7), or "" when it has none. */
const char *tab_names_get (int place);

/** Give a place (0 to 7) a name, or take it away with "" or NULL. Saved at once. */
void tab_names_set (int place, const char *name);

/** The folder a place (0 to 7) shows as a folder tab ("/Utilities"), or "" when it has none. */
const char *tab_names_folder (int place);

/** Set the folder a place shows ("/Utilities"), or take it away with "" or NULL. Saved at once. */
void tab_names_set_folder (int place, const char *folder);

#endif /* TAB_NAMES_H__ */
