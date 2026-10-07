/**
 * @file game_info_ui.h
 * @brief The redesigned Game info screen: the cover art stretched and
 *        darkened as the background, a big title, badges, a few facts in
 *        boxes, the description and button hints.
 *
 * Only the main page is drawn here. The pop-ups (extra info, advanced info,
 * the Expansion Pak warning, the options menu) stay with the stock screen.
 */

#ifndef GAME_INFO_UI_H__
#define GAME_INFO_UI_H__

#include "menu_state.h"
#include "ui_components.h"

/** Text the stock screen already knows how to produce, handed over as is. */
typedef struct {
    const char *name;           /**< the name to show at the top */
    const char *description;    /**< short description, or a placeholder */
    const char *save;           /**< save type in words */
    const char *tv;             /**< TV region in words */
    bool cheats;                /**< Datel cheats switched on for this game */
    bool patches;               /**< patches switched on for this game */
    bool clear_rdram;           /**< "clear memory before start" switched on */
    bool front_picture;         /**< the picture being shown is the front cover */
} game_info_view_t;

/**
 * Draw the main page. Call after the shared background has been drawn.
 * @param art The box art being shown, or NULL if there is none (yet).
 */
void game_info_ui_draw (menu_t *menu, component_boxart_t *art, const game_info_view_t *view);

/**
 * True if the backdrop for the game being shown is ready. It covers the whole
 * screen, so the shared background underneath need not be drawn.
 */
bool game_info_ui_backdrop_ready (menu_t *menu);

#endif /* GAME_INFO_UI_H__ */
