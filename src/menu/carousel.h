/**
 * @file carousel.h
 * @brief The row of covers, as drawn on the Games screen.
 *
 * The drawing code lives in views/browser.c, where it began. It shows
 * whichever list cover_list.h says is current, so the Recent tab can use it.
 */

#ifndef CAROUSEL_H__
#define CAROUSEL_H__

#include "menu_state.h"

/** Draw the covers, the selection ring, the title panel and the position bar. */
void carousel_draw (menu_t *menu);

#endif /* CAROUSEL_H__ */
