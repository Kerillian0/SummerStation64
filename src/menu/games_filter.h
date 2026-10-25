/**
 * @file games_filter.h
 * @brief Shows only one region's games, only 64DD disks or only hacks, on the Games tab.
 *
 * C-up and C-down step through All, USA, Japan, Europe, 64DD, Hacks and Party (party.h), leaving out
 * any with no games. A game's region comes from its code in the remembered
 * game list (the last letter: E USA, J Japan, P and the other PAL letters
 * Europe), or else from tags in its file name such as (U), (J), (E), (Japan).
 * A game whose region can't be told shows under All only. Lasts while the
 * console is on.
 *
 * The games that don't match are kept at the end of the list, past
 * `menu->browser.entries`, so changing the filter reads nothing from the card.
 */

#ifndef GAMES_FILTER_H__
#define GAMES_FILTER_H__

#include <stdbool.h>
#include "menu_state.h"

/**
 * After the Games tab's list is read, before it is sorted: work out each
 * game's region and hide what the filter leaves out. `compare` is the
 * browser's own sort, used when the filter changes.
 */
void games_filter_after_load (menu_t *menu, int (*compare) (const void *, const void *));

/**
 * Step to the next (+1) or previous (-1) filter that has games, keeping the
 * selected game if it is in the new set.
 * @return false if nothing changed (no other filter has games).
 */
bool games_filter_step (menu_t *menu, int direction);

/** Once a frame on the Games tab: if controllers came or went while Party shows, show the new set. */
void games_filter_party_check (menu_t *menu);

/**
 * A game's code was just learned: place it in its region, if its file name
 * gave none. It shows under that filter the next time the filter is chosen.
 */
void games_filter_note_code (const entry_t *e, const char *code);

/** True if this game of the Games tab's list is a hack of another game (see rom_hacks.h). */
bool games_filter_is_hack (const entry_t *e);

/** The same, for a full path ("sd:/..."), looked up in the Games list (false if it isn't there). */
bool games_filter_is_hack_path (menu_t *menu, const char *path);

/**
 * True if this full path's game shares its cover with other regions' copies
 * of it (Region Copies: One Cover, see region_pick.h).
 */
bool games_filter_region_copies (menu_t *menu, const char *path);

/**
 * Work out which games are hacks again, now that more game codes are known
 * (called when the idle reader has learned some). Reads nothing from the card.
 */
void games_filter_recheck_hacks (menu_t *menu);

/** How many entries the list really holds, the hidden ones included (for freeing it). */
int games_filter_all_entries (int shown);

/** The filter's name for the screen, or NULL while it is All. */
const char *games_filter_label (void);

/**
 * The Homebrew tab: the Games list showing only homebrew (games whose code
 * homebrew tools write, game_index_is_homebrew()). C-up / C-down do nothing
 * there. The Retro tab: only emulator games (retro.h), which the Games tab
 * leaves out; C-up / C-down step through its systems. The Games tab's own
 * step comes back when either is left. Homebrew the
 * idle reader finds while the shelf shows is added when it finishes.
 * @param list_loaded The Games list is loaded now (else the next load applies it).
 */
typedef enum { GAMES_SHELF_NONE, GAMES_SHELF_HOMEBREW, GAMES_SHELF_RETRO } games_shelf_t;

void games_filter_shelf (menu_t *menu, games_shelf_t which, bool list_loaded);

/**
 * The Collections tab: the Games list showing one collection (by its number,
 * collections_id()), or `which` -1 to leave it (back to the Games tab's own step).
 * @param list_loaded The Games list is loaded now (else the next load applies it).
 */
void games_filter_collection (menu_t *menu, int which, bool list_loaded);

/** The player changed a collection of their own: work the collections out again (shows at once in an open one). */
void games_filter_collections_changed (menu_t *menu);

/**
 * How many covers a collection has with the current folding (Game Versions,
 * Region Copies), putting the first `most` of them, in list order, in `out`.
 */
int games_filter_collection_covers (menu_t *menu, int which, entry_t **out, int most);

/** Game Versions (option `game_versions`, Settings > Library). */
typedef enum {
    GAME_VERSIONS_SEPARATE,     /**< every file its own cover (the default) */
    GAME_VERSIONS_GROUPED,      /**< one cover per game code: the original, its hacks and copies */
    GAME_VERSIONS_COUNT
} game_versions_t;

/** Name of a game_versions_t choice, for Settings. */
const char *games_filter_versions_name (int choice);

/**
 * With Game Versions grouped, or Region Copies on One Cover under All: how
 * many files this cover stands for (1 when it stands for itself only, or
 * nothing is folded for the filter shown).
 */
int games_filter_versions (menu_t *menu, const entry_t *e);

/**
 * The files a cover stands for, its own first in list order, at most `most`.
 * @return how many were put in `out`.
 */
int games_filter_version_list (menu_t *menu, const entry_t *e, entry_t **out, int most);

/**
 * Which file's cover this one shows under for the filter in use (its own
 * entry->index when nothing folds it, or the list is not the Games list).
 */
int games_filter_cover_index (const entry_t *e);

/** Forget the hidden entries (the list was freed or is another folder's). */
void games_filter_forget (void);

#endif /* GAMES_FILTER_H__ */
