/**
 * @file tab_names.h
 * @brief Names of the player's own for the places on the tab bar.
 *
 * Settings > Games > Tabs: each place (First Tab ... Eighth Tab) can be
 * given a name, typed with the naming keyboard; the tab shows it instead of
 * its usual one ("Utilities" for a folder of tools, "Zelda" for a
 * collection). The name belongs to the place, so it stays when the place is
 * set to show something else. An empty name gives the tab back its usual
 * one. Kept per player in menu/tabnames.txt (`tabnames 1`, then
 * `<place 1-8> <name>` lines).
 */

#ifndef TAB_NAMES_H__
#define TAB_NAMES_H__

/** The longest name a tab can have (a tab shows about 10 letters; longer ones end in ...). */
#define TAB_NAME_MAX    (16)

/** The name given to a place (0 to 7), or "" when it has none. */
const char *tab_names_get (int place);

/** Give a place (0 to 7) a name, or take it away with "" or NULL. Saved at once. */
void tab_names_set (int place, const char *name);

#endif /* TAB_NAMES_H__ */
