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
#include "fonts.h"

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

/** The same for the game at this full path (the 64DD and Retro screens). */
bool game_info_ui_backdrop_ready_for (const char *path);

#define GAME_INFO_PAGE_BADGES   (3)

/**
 * A game other than an N64 cartridge (a 64DD disk, an emulator game), drawn
 * the same way: backdrop, cover, the line above the title, the title,
 * badges, Played and Last played (play_stats.h) and one box of its own,
 * the description and the button hints.
 */
typedef struct {
    const char *path;           /**< the game's full path: its play counts, and whose backdrop is kept */
    const char *name;
    const char *above;          /**< the grey line above the title (genre, date, maker), or NULL */
    const char *badges[GAME_INFO_PAGE_BADGES];      /**< up to three, NULL after the last */
    menu_font_style_t badge_styles[GAME_INFO_PAGE_BADGES];
    const char *fact_caption;   /**< the third box */
    const char *fact_value;
    const char *description;    /**< NULL: "No description available." */
    const char *a_hint;         /**< NULL: "Play" */
    const char *r_hint, *z_hint, *start_hint;   /**< NULL: not shown */
    bool pictures;              /**< ◀▶ Pictures */
    bool front_picture;         /**< the picture shown is the front cover (the backdrop is made from it) */
} game_info_page_t;

void game_info_ui_draw_page (menu_t *menu, component_boxart_t *art, const game_info_page_t *page);

/** "1998-10-26" -> "Oct 26, 1998" (or as it was, if it isn't a date). */
void game_info_ui_format_date (char *out, size_t size, const char *text);

#endif /* GAME_INFO_UI_H__ */
