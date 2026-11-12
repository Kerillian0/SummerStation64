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
    ASSISTANT_USED_STATUS,          /* a status mark set (game_status.h) */
    ASSISTANT_USED_LIBRARY_STATS,   /* Library Stats opened */
    ASSISTANT_USED_SETTINGS_SEARCH, /* a setting searched for */
    ASSISTANT_USED_NEVER_PLAYED,    /* the Never Played step reached */
    ASSISTANT_USED_TEST_PATTERNS,
    ASSISTANT_USED_PAD_TEST,
    ASSISTANT_USED_COUNT
} assistant_used_t;

/** Names of the choices, for Settings. */
const char *assistant_often_name (int choice);
const char *assistant_mood_name (int choice);
const char *assistant_color_name (int choice);
/** How many values Fairy Color has: its own, the theme's accent, then named colours. */
#define ASSISTANT_COLOR_COUNT  (12)

/** How it comes and goes, moves while it stays, and how its glow breathes (Fairy Look). */
#define ASSISTANT_ARRIVAL_COUNT (4)
#define ASSISTANT_IDLE_COUNT    (4)
#define ASSISTANT_GLOW_COUNT    (3)
const char *assistant_arrival_name (int choice);
const char *assistant_idle_name (int choice);
const char *assistant_glow_name (int choice);

/** The fairies to choose from: 0 is Vee, the menu's own (named by the player, if they did); 1 on, the skins
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

/** The longest name the player may give their fairy. */
#define ASSISTANT_NAME_MAX  (16)

/** Give the fairy in use a name of the player's (per player); empty or NULL gives it back its own. */
void assistant_rename (const char *name);

/** For the Ask page (assistant_ask.c): the fairy in use, its colour and mood. */
const char *assistant_name (void);
color_t assistant_color (void);
bool assistant_snarky (void);

/** The fairy's picture, its middle at x, y, `size` pixels high (loaded the
 *  first time); free it when the page closes. */
void assistant_portrait_draw (int x, int y, int size);
void assistant_portrait_free (void);

/** The fairy at x, y as it is, `size` pixels high, `alpha` faded (no arrival:
 *  the intro flies it itself). Uses the portrait's picture: assistant_portrait_free() after. */
void assistant_fairy_draw (float x, float y, int size, uint8_t alpha);

/** One of its sparkles, `size` pixels across, in its colour or white. */
void assistant_star_draw (float x, float y, float size, uint8_t alpha, bool white);

#endif
