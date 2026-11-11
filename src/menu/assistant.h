/**
 * @file assistant.h
 * @brief The menu assistant: a fairy that flies in now and then with a tip
 * or a remark in a speech bubble, then flies away. Any button sends it off.
 * Settings > Display: Assistant (how often, or Off) and Assistant Mood
 * (Eager or Snarky). What it has said, and which features the player has
 * used (so it stops suggesting them), is kept per player in
 * menu/assistant.txt.
 */

#ifndef ASSISTANT_H__
#define ASSISTANT_H__

#include <libdragon.h>
#include "menu_state.h"

typedef enum {
    ASSISTANT_OFF,
    ASSISTANT_RARELY,
    ASSISTANT_NOW_AND_THEN,
    ASSISTANT_CHATTY,
    ASSISTANT_COUNT
} assistant_often_t;

typedef enum {
    ASSISTANT_EAGER,
    ASSISTANT_SNARKY,
    ASSISTANT_MOOD_COUNT
} assistant_mood_t;

/** Features the tips suggest, noted where they are used. */
typedef enum {
    ASSISTANT_USED_LETTER_DIAL,
    ASSISTANT_USED_FILTER,
    ASSISTANT_USED_SCREENSHOT,
    ASSISTANT_USED_RANDOM,
    ASSISTANT_USED_DETAILS,
    ASSISTANT_USED_THEME_EDITOR,
    ASSISTANT_USED_INSPECT,
    ASSISTANT_USED_COUNT
} assistant_used_t;

/** Names of the choices, for Settings. */
const char *assistant_often_name (int choice);
const char *assistant_mood_name (int choice);

/** The fairies to choose from: 0 is Spark, the menu's own; 1 on, the skins
 *  baked into the menu file ("" past the last). */
#define ASSISTANT_SKIN_COUNT    (9)
const char *assistant_skin_name (int choice);

/** The player has used one of the features the tips suggest. */
void assistant_note_used (assistant_used_t what);

/** Forget which tips were said, so they are all said again (Settings). */
void assistant_reset (void);

/** Once a frame (menu.c): whether it is time to say something. */
void assistant_update (menu_t *menu);

/** On the screens it can appear on, the last thing drawn before the
 *  Performance Overlay. */
void assistant_draw (menu_t *menu);

/** For the Ask page (assistant_ask.c): the fairy in use, its colour and mood. */
const char *assistant_name (void);
color_t assistant_color (void);
bool assistant_snarky (void);

/** The fairy's picture, its middle at x, y, `size` pixels high (loaded the
 *  first time); free it when the page closes. */
void assistant_portrait_draw (int x, int y, int size);
void assistant_portrait_free (void);

#endif
