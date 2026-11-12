/**
 * @file settings_search.h
 * @brief Finding a setting by words (Z in Settings): does a row's name, or
 * its name and help together, hold every word typed? No console code, so it
 * is tested on the PC (tests/settings_search_test.c).
 *
 * Words are told apart by spaces; case doesn't matter, and a word may be
 * part of a longer one ("beat" finds "beaten").
 */

#ifndef SETTINGS_SEARCH_H__
#define SETTINGS_SEARCH_H__

typedef enum {
    SETTINGS_SEARCH_NONE,   /**< some word is in neither */
    SETTINGS_SEARCH_HELP,   /**< every word is in the name or the help, not all in the name */
    SETTINGS_SEARCH_NAME,   /**< every word is in the name */
} settings_search_match_t;

/** How a row matches what was typed. A query with no words matches nothing. */
settings_search_match_t settings_search_match (const char *query, const char *name, const char *help);

#endif /* SETTINGS_SEARCH_H__ */
