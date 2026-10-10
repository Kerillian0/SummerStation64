/**
 * @file theme_parse.h
 * @brief Reads a theme.ini file into a theme.
 *
 * Kept apart from theme.c, which also draws the background, so the PC-side
 * tests (tests/) can build and check it without the console's libraries.
 */

#ifndef THEME_PARSE_H__
#define THEME_PARSE_H__

#include <stdbool.h>
#include "theme.h"

/**
 * Read `path` and apply each key it sets to `t`. Keys it leaves out, and
 * values that are not valid, leave `t` as it was (so fill in the defaults
 * first).
 * @return false if the file could not be opened.
 */
bool theme_parse_file (theme_t *t, const char *path);

#endif /* THEME_PARSE_H__ */
