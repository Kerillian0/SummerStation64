/**
 * @file font_choice.h
 * @brief Which built-in font to load: the full one or the small Latin one.
 *
 * The full font covers Japanese file names but takes about 760 KB of RAM.
 * The small one has Latin characters only and takes about 64 KB, which
 * matters on a console without the Expansion Pak.
 */

#ifndef FONT_CHOICE_H__
#define FONT_CHOICE_H__

typedef enum {
    FONT_AUTO,      /**< small without the Expansion Pak, full with it */
    FONT_FULL,
    FONT_SMALL,
    FONT_COUNT
} font_choice_t;

/** Label for the Settings screen. */
const char *font_choice_name (int choice);

/** Path of the built-in font to load, following the player's choice. */
const char *font_choice_path (void);

#endif /* FONT_CHOICE_H__ */
