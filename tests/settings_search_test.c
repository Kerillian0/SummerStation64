/* PC-side tests for finding a setting by words (src/menu/settings_search.c).
   Run with tests/run.sh. */

#include <stdio.h>

#include "settings_search.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static const char *progress = "Collection Progress";
static const char *progress_help = "On the Collections tab, a card says how many of its games are beaten (Beaten or 100%, from Game Status).";

int main (void) {
    CHECK(settings_search_match("progress", progress, progress_help) == SETTINGS_SEARCH_NAME, "a word of the name");
    CHECK(settings_search_match("PROGRESS", progress, progress_help) == SETTINGS_SEARCH_NAME, "case doesn't matter");
    CHECK(settings_search_match("coll prog", progress, progress_help) == SETTINGS_SEARCH_NAME, "parts of two words of the name");
    CHECK(settings_search_match("beaten", progress, progress_help) == SETTINGS_SEARCH_HELP, "a word only in the help");
    CHECK(settings_search_match("collection beaten", progress, progress_help) == SETTINGS_SEARCH_HELP, "one word in the name, one in the help");
    CHECK(settings_search_match("beaten zelda", progress, progress_help) == SETTINGS_SEARCH_NONE, "every word is needed");
    CHECK(settings_search_match("100%", progress, progress_help) == SETTINGS_SEARCH_HELP, "symbols match too");
    CHECK(settings_search_match("  progress   ", progress, progress_help) == SETTINGS_SEARCH_NAME, "spaces round the words");
    CHECK(settings_search_match("", progress, progress_help) == SETTINGS_SEARCH_NONE, "no words, no match");
    CHECK(settings_search_match("   ", progress, progress_help) == SETTINGS_SEARCH_NONE, "only spaces, no match");
    CHECK(settings_search_match(NULL, progress, progress_help) == SETTINGS_SEARCH_NONE, "no query at all");
    CHECK(settings_search_match("status", "Game Status", NULL) == SETTINGS_SEARCH_NAME, "a row without help");
    CHECK(settings_search_match("blur", "Game Status", NULL) == SETTINGS_SEARCH_NONE, "and no help to look in");
    CHECK(settings_search_match("progressive", progress, progress_help) == SETTINGS_SEARCH_NONE, "a longer word isn't in a shorter one");
    printf("settings search: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
