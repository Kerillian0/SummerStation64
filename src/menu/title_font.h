/**
 * @file title_font.h
 * @brief The larger font used for game titles.
 *
 * Same typeface as the body text at 26 px and at 20 px, with English
 * letters, digits, punctuation and Western European accents only.
 */

#ifndef TITLE_FONT_H__
#define TITLE_FONT_H__

#include <stdbool.h>
#include <stdint.h>

/** Load and register the title fonts (FNT_TITLE, FNT_TITLE_MEDIUM) and the small one (FNT_SMALL). Call once, with the other fonts. */
void title_font_init (void);

/**
 * Which font to draw a title with: the largest of FNT_TITLE and
 * FNT_TITLE_MEDIUM in which the text fits in `max_width` on one line. If it
 * fits in neither, or uses a character they lack, FNT_DEFAULT (which has
 * more characters and is narrower).
 */
uint8_t title_font_pick (const char *text, int max_width);

#endif /* TITLE_FONT_H__ */
