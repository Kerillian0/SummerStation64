/**
 * @file game_start.h
 * @brief What shows while a game starts (option `game_start`, Settings >
 * Look > Start-up > Game Start).
 *
 * Box Opens: when A starts a game on Game info, its cover flies to the
 * middle as a 3D box, the box's top flap opens and the cartridge pops up out
 * of it (the "opening", drawn by Game info's own frames while the music
 * fades out). Then, while the game is copied into the cart, the box drops
 * away and the cartridge spins to the middle of the screen, growing, stops
 * on its label, winds up and snaps into the screen's bottom edge, and the
 * picture fades to black. The animation keeps its own time (about 2.9 s in
 * all); for a game that loads slowly the spin and the label slow down to
 * match, and the snap waits for the loading to finish.
 *
 * Loading the game overwrites the cart's copy of the menu file, where the
 * menu's own files (rom:/) are read from, so everything the animation draws
 * is in memory before the copying starts, and nothing reads rom:/ or plays
 * sound until the game runs. Loading Bar is the stock bar.
 */

#ifndef GAME_START_H__
#define GAME_START_H__

#include <stdbool.h>
#include "menu_state.h"
#include "ui_components.h"

typedef enum {
    GAME_START_BOX,     /**< the box opens, the cartridge goes in */
    GAME_START_BAR,     /**< the stock loading bar */
    GAME_START_COUNT
} game_start_t;

/** Name of an option `game_start` value, for Settings. */
const char *game_start_name (int choice);

/**
 * A was pressed on Game info: start the opening, with this cover.
 * @return false when the plain bar is wanted (load at once, as before).
 */
bool game_start_begin (menu_t *menu, component_boxart_t *cover);

/** The opening is playing: Game info takes no buttons, and draws it with game_start_draw(). */
bool game_start_opening (void);

/** The opening has finished: time to load the game. */
bool game_start_ready (void);

/** Draw the animation as it is now (on Game info's frame; the page under it fades out first). */
void game_start_draw (menu_t *menu);

/** The page is gone: Game info need not draw it under the animation. */
bool game_start_page_gone (void);

/** Loading starts now (after the save backup): the sound stops. @return false if no animation is playing. */
bool game_start_load_begin (void);

/** The loader's progress (0 to 1): draws a frame when one is free. */
void game_start_progress (float progress);

/** The game is loaded: play the rest, ending on black. */
void game_start_finish (void);

/** Let go of everything (after the finish, or when loading failed). */
void game_start_end (void);

#endif
