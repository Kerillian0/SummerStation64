/**
 * @file assistant_ask.h
 * @brief "Ask a Question": a page of Settings where the assistant answers
 * questions about the menu, for players who'd rather not open the web
 * guide. Topics on the left, their questions beside them; A shows the
 * answer in a panel with the fairy perched on it, a short line in the
 * Eager or Snarky mood first. The questions and answers are
 * rom:/ask.txt (assets/data/ask.txt), read when the page opens and freed
 * when it closes.
 */

#ifndef ASSISTANT_ASK_H__
#define ASSISTANT_ASK_H__

#include <stdbool.h>
#include "menu_state.h"

/** Open the page. `from_tabs`: opened from a cover tab's Z menu, so closing it leaves Settings. */
void assistant_ask_open (bool from_tabs);

/** Whether the page is showing (Settings draws and runs it instead of its own page). */
bool assistant_ask_active (void);

void assistant_ask_process (menu_t *menu);
void assistant_ask_draw (void);

#endif
