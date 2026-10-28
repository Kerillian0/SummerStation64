/**
 * @file profiles.h
 * @brief Up to four players, each with their own saves.
 *
 * The list lives in sd:/menu/profiles.txt. The first profile is the one the
 * menu always had: its saves stay where they were (<game folder>/saves/).
 * Every other profile keeps its saves in a folder of its own beside them
 * (<game folder>/saves/<profile folder>/), so a new profile starts each
 * game with a fresh save and never touches another player's.
 *
 * Each player also has their own Recent, Favorites, play counts, own
 * collections, hidden games, region picks and folder places: the first
 * profile in sd:/menu/ as always, the others in sd:/menu/profiles/<folder>/.
 *
 * A profile's folder is named after the player when it is made and keeps
 * that name when the profile is renamed. Deleting a profile only takes it
 * off the list: its saves stay on the card, and its folder name is never
 * given to a new profile.
 */

#ifndef PROFILES_H__
#define PROFILES_H__

#include <stdbool.h>
#include <libdragon.h>

#include "path.h"

#define PROFILES_MAX        (4)
#define PROFILE_COLORS      (4)

/** How many profiles there are (at least one). */
int profiles_count (void);

/** Which profile is playing (0 to profiles_count() - 1). */
int profiles_current (void);

/** A profile's name. */
const char *profiles_name (int profile);

/** A profile's color: one of PROFILE_COLORS, as a color and as its number. */
color_t profiles_color (int profile);
int profiles_color_index (int profile);

/** Change a profile's color to the next one. */
void profiles_next_color (int profile);

/** Play as another profile (saved to the card at once). */
void profiles_switch (int profile);

/** Add a profile with this name; false when there are four already. It is not switched to. */
bool profiles_add (const char *name);

/** Give a profile a new name (its saves folder keeps its first name). */
void profiles_rename (int profile, const char *name);

/** Take a profile off the list (never the first); its saves stay on the card. */
bool profiles_delete (int profile);

/** The playing profile's copy of one of the menu's list files: "history.ini" is sd:/menu/history.ini for the first profile, sd:/menu/profiles/<folder>/history.ini for the others. A few may be in use at once. */
const char *profiles_file (const char *name);

/** A number that changes whenever the player changes: a list loaded under another number was the last player's and must be read again. */
int profiles_generation (void);

/**
 * Point a save file's path at the playing profile's own saves folder:
 * "N64/saves/Game.sav" becomes "N64/saves/<folder>/Game.sav" (unchanged for
 * the first profile). Call after the saves folder has been added.
 * @param create Make the folder if it isn't there (when a game is started).
 * @return false if the folder couldn't be made.
 */
bool profiles_save_path (path_t *path, bool create);

/** The saves folder of every profile but the first, for moving a renamed game's saves along: NULL past the last. */
const char *profiles_folder_of (int profile);

#endif /* PROFILES_H__ */
