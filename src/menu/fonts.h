/**
 * @file fonts.h
 * @brief Menu fonts
 * @ingroup menu 
 */

#ifndef FONTS_H__
#define FONTS_H__

/**
 * @brief Font type enumeration.
 * 
 * This enumeration defines the different types of fonts that can be used
 * in the menu system.
 */
typedef enum {
    FNT_DEFAULT = 1, /**< Default font type */
    FNT_TITLE = 2,   /**< Larger font for game titles (see title_font.h) */
    FNT_TITLE_MEDIUM = 3, /**< In-between size, for titles too long for FNT_TITLE */
    FNT_SMALL = 4,   /**< 12 px, English letters only, for a second line under another (see title_font.h) */
} menu_font_type_t;

/**
 * @brief Font style enumeration.
 * 
 * This enumeration defines the different styles of fonts that can be used
 * in the menu system.
 */
typedef enum {
    STL_DEFAULT = 0, /**< Default font style */
    STL_GREEN,       /**< Green font style */
    STL_BLUE,        /**< Blue font style */
    STL_YELLOW,      /**< Yellow font style */
    STL_ORANGE,      /**< Orange font style */
    STL_RED,         /**< Red font style */
    STL_GRAY,        /**< Gray font style */
    STL_WHITE,       /**< Always white, whatever the theme (text on the button icons) */
    STL_BLACK,       /**< Always black (text on a light accent color) */
} menu_font_style_t;

/**
 * @brief Initialize fonts.
 * 
 * This function initializes the fonts used in the menu system. It can load
 * custom fonts from the specified path.
 * 
 * @param custom_font_path Path to the custom font file.
 */
void fonts_init(char *custom_font_path);

/** Give the text the current theme's colors again, after the theme changes. */
void fonts_restyle (void);

#endif /* FONTS_H__ */
