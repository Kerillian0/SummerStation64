/**
 * @file name_keyboard.h
 * @brief An on-screen keyboard for naming things, the other choice to the dial.
 *
 * Laid out like a keyboard (number row, QWERTY letters, the symbols a file
 * name may hold in the spare keys) with Erase, Shift and OK down the right,
 * after the Switch's. The D-Pad or stick moves, A types the key, B erases
 * (at the start of the name it gives up), R is Shift (once: one capital,
 * twice: caps lock), C-down types a space, C-left / C-right move the text
 * cursor, START or OK keeps the name.
 *
 * Option `name_entry` (Settings > Controls, "Name Entry") picks it or the
 * dial. The dial (letter_wheel.c) keeps the name and hands it here, so
 * every screen that asks for a name gets the keyboard without a change.
 */

#ifndef NAME_KEYBOARD_H__
#define NAME_KEYBOARD_H__

#include "menu_state.h"

/** Option `name_entry` in options.ini. */
typedef enum {
    NAME_ENTRY_KEYBOARD,    /**< the default */
    NAME_ENTRY_DIAL,
    NAME_ENTRY_COUNT
} name_entry_t;

/** The name of a choice, for the settings screen. */
const char *name_keyboard_entry_name (int choice);

typedef enum {
    NAME_KEYBOARD_TYPING,
    NAME_KEYBOARD_DONE,     /**< START or OK with something typed */
    NAME_KEYBOARD_CANCEL,   /**< B at the start of the name */
} name_keyboard_result_t;

/** Get ready for a new name (`text` is what it starts with). */
void name_keyboard_begin (const char *text);

/** Read this frame's buttons and change `text` (at most `limit` characters) and `*cursor`. */
name_keyboard_result_t name_keyboard_process (menu_t *menu, char *text, int *cursor, int limit);

/** Draw the keyboard with the name so far. */
void name_keyboard_draw (const char *prompt, const char *text, int cursor, int limit);

/** Let go of what it keeps while open. */
void name_keyboard_end (void);

#endif /* NAME_KEYBOARD_H__ */
