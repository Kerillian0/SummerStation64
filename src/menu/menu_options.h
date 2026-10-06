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
    OPTION_COUNT
} option_t;

/** Current value of an option (its default if never set). */
int options_get (option_t option);

/** Change an option and save it to the SD card. */
void options_set (option_t option, int value);

#endif /* MENU_OPTIONS_H__ */
