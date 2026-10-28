/**
 * @file menu_options.h
 * @brief The player's own menu preferences that are not on/off switches.
 *
 * On/off switches that a theme may also set live in menu_features. These are
 * personal choices with more than two values, saved to options.ini.
 */

#ifndef MENU_OPTIONS_H__
#define MENU_OPTIONS_H__

typedef enum {
    OPTION_SORT_ORDER,  /**< a sort_order_t value */
    OPTION_FONT,        /**< a font_choice_t value */
    OPTION_FRAME_RATE,  /**< a frame_rate_t value */
    OPTION_INTRO,       /**< an intro_choice_t value (see intro.h) */
    OPTION_FADE,        /**< an intro_fade_t value (see intro.h) */
    OPTION_THEME,       /**< a builtin_theme_t value (see builtin_themes.h), or a card theme from THEME_CHOICE_CARD on (card_themes.h) */
    OPTION_INTRO_LOGO,  /**< an intro_logo_t value (see intro_logo.h) */
    OPTION_TAB1,        /**< first place on the tab bar: a tab_choice_t value (see tabs.h) */
    OPTION_TAB2,
    OPTION_TAB3,
    OPTION_TAB4,
    OPTION_RING_TINT,   /**< a ring_tint_t value (see art_tint.h) */
    OPTION_PERF_OVERLAY, /**< 1: show frame time and free memory on screen (see debug_stats.h) */
    OPTION_PAK_BADGES,  /**< a pak_badges_t value (see games_ui.h) */
    OPTION_SELECTION,   /**< a selection_style_t value (see games_ui.h) */
    OPTION_BOX_HOVER,   /**< a box_hover_t value (see box_motion.h) */
    OPTION_COVER_STYLE, /**< a cover_style_t value (see carousel_art.h) */
    OPTION_GAME_VERSIONS, /**< a game_versions_t value (see games_filter.h) */
    OPTION_REGION_COPIES, /**< a region_copies_t value (see region_pick.h) */
    OPTION_PREFERRED_REGION, /**< a preferred_region_t value (see region_pick.h) */
    OPTION_SHOT_SIZE,   /**< a screenshot_size_t value (see screenshot.h) */
    OPTION_SHOT_DITHER, /**< a screenshot_dither_t value (see screenshot.h) */
    OPTION_SHOT_BUTTON, /**< a screenshot_button_t value (see screenshot.h) */
    OPTION_CLIP_LENGTH, /**< a clip_length_t value (see clip.h) */
    OPTION_CLIP_SIZE,   /**< a screenshot_size_t value, for clips */
    OPTION_CLIP_FPS,    /**< a clip_fps_t value */
    OPTION_CLIP_DITHER, /**< a screenshot_dither_t value, for clips */
    OPTION_HOMEBREW_ON_GAMES, /**< a homebrew_on_games_t value (see games_filter.h) */
    OPTION_HACKS_ON_GAMES, /**< a hacks_on_games_t value (see games_filter.h) */
    OPTION_THEME_KEY,   /**< which card theme option `theme` means, by its folder name (see card_themes.h) */
    OPTION_MUSIC_VOLUME, /**< 0-10, see volume.h */
    OPTION_SOUND_VOLUME, /**< 0-10, see volume.h */
    OPTION_PARTY_TEST,  /**< hidden, for testing: count this many controllers (see party.h); 0 counts the real ones */
    OPTION_COUNT
} option_t;

/** Current value of an option (its default if never set). */
int options_get (option_t option);

/** Change an option and save it to the SD card. */
void options_set (option_t option, int value);

/** Change an option in memory only; options_flush() saves it later. */
void options_change (option_t option, int value);

/** Save changes made with options_change(), if there are any. */
void options_flush (void);

#endif /* MENU_OPTIONS_H__ */
