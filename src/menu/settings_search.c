#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "settings_search.h"

static char lower (char c) {
    return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
}

/* Is this word (`length` characters) anywhere in the text, case left aside? */
static bool holds (const char *text, const char *word, size_t length) {
    if (!text) {
        return false;
    }
    for (const char *t = text; *t; t++) {
        size_t i = 0;
        while (i < length && t[i] && lower(t[i]) == lower(word[i])) {
            i++;
        }
        if (i == length) {
            return true;
        }
    }
    return false;
}

settings_search_match_t settings_search_match (const char *query, const char *name, const char *help) {
    bool any = false, all_in_name = true;
    const char *q = query ? query : "";
    while (*q) {
        while (*q == ' ') {
            q++;
        }
        size_t length = strcspn(q, " ");
        if (length == 0) {
            break;
        }
        any = true;
        bool in_name = holds(name, q, length);
        if (!in_name && !holds(help, q, length)) {
            return SETTINGS_SEARCH_NONE;
        }
        all_in_name &= in_name;
        q += length;
    }
    return !any ? SETTINGS_SEARCH_NONE : all_in_name ? SETTINGS_SEARCH_NAME : SETTINGS_SEARCH_HELP;
}
