/**
 * @file letter_wheel.h
 * @brief Hold Z on the Games or Homebrew tab to find a game by its name.
 *
 * A round dial of letters (#, A-Z) opens over the covers while Z is held.
 * The stick points at a letter and C-left / C-right step round to the
 * previous or next one; the covers jump at once to the first game whose
 * name starts with what is chosen. A adds the letter pointed at, so names
 * can be spelled out ("SU", then "SUP"); B takes the last letter back, or
 * with none left goes back to the game the dial opened on. Letting go of Z
 * keeps the place. A tap of Z still opens Options.
 *
 * Names are matched as shown, without a leading "The", "A" or "An", spaces
 * or punctuation ("The Legend of Zelda" is found under L, "F-Zero X" as
 * FZEROX); # stands for a digit. Letters no game has are dimmed and
 * skipped.
 */

#ifndef LETTER_WHEEL_H__
#define LETTER_WHEEL_H__

#include <stdbool.h>
#include "menu_state.h"

typedef enum {
    LETTER_WHEEL_IDLE,      /**< Z isn't held: the screen handles its buttons as usual */
    LETTER_WHEEL_BUSY,      /**< Z is held: the dial has the controls this frame */
    LETTER_WHEEL_TAP,       /**< Z was tapped: open Options */
} letter_wheel_state_t;

/**
 * Read Z and, while it is held, the stick, C-left / C-right, A and B.
 * Only on the cover row of the Games list (Games and Homebrew tabs);
 * elsewhere it returns LETTER_WHEEL_IDLE and leaves Z alone.
 */
letter_wheel_state_t letter_wheel_process (menu_t *menu);

/** Draw the dial while it is open. */
void letter_wheel_draw (void);

/** Close it without drawing again (the list is being freed or the screen left). */
void letter_wheel_close (void);

/**
 * Spell a name with the dial (a new collection's, for one): it stays open
 * by itself until START keeps the name (in Title Case) or B with nothing
 * left gives up, and then calls `done` with the name, or NULL.
 * @param prompt Shown above the name ("New collection").
 * @param start The name to start from, or NULL.
 */
void letter_wheel_name_begin (const char *prompt, const char *start, void (*done) (menu_t *menu, const char *name));

/** True while a name is being spelled: the screen hands its buttons to letter_wheel_name_process(). */
bool letter_wheel_naming (void);

/** Read this frame's buttons while naming. */
void letter_wheel_name_process (menu_t *menu);

#endif /* LETTER_WHEEL_H__ */
