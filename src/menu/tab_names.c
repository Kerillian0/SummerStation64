/**
 * @file tab_names.c
 * @brief Names of the player's own for the places on the tab bar.
 */

#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "profiles.h"
#include "safe_file.h"
#include "tab_names.h"
#include "tabs.h"

#define NAMES_PATH      profiles_file("tabnames.txt")   /* the playing profile's */
#define NAMES_TMP_PATH  profiles_file("tabnames.tmp")
#define NAMES_HEADER    "tabnames 1"

static char names[TABS_PLACES][TAB_NAME_MAX + 1];
static bool loaded = false;
static int loaded_for = 0;      /* profiles_generation() when loaded */

static void load (void) {
    if (loaded && loaded_for == profiles_generation()) {
        return;
    }
    loaded = true;
    loaded_for = profiles_generation();     /* another player: forget the last one's */
    memset(names, 0, sizeof(names));

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(NAMES_PATH, "r");
    if (!f) {
        f = fopen(NAMES_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }
    char line[64];
    bool header = false;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!header) {
            header = (strcmp(line, NAMES_HEADER) == 0);
            if (!header) {
                break;      /* not a file the menu wrote */
            }
            continue;
        }
        int place = line[0] - '1';
        if (place >= 0 && place < TABS_PLACES && line[1] == ' ' && line[2]) {
            snprintf(names[place], sizeof(names[0]), "%s", line + 2);
        }
    }
    fclose(f);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (void) {
    FILE *f = fopen(NAMES_TMP_PATH, "w");
    if (!f) {
        debugf("tab names: could not write %s\n", NAMES_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", NAMES_HEADER);
    for (int i = 0; i < TABS_PLACES; i++) {
        if (names[i][0]) {
            fprintf(f, "%d %s\n", i + 1, names[i]);
        }
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(NAMES_TMP_PATH, NAMES_PATH)) {
        debugf("tab names: could not replace %s\n", NAMES_PATH);
    }
}

const char *tab_names_get (int place) {
    if (place < 0 || place >= TABS_PLACES) {
        return "";
    }
    load();
    return names[place];
}

void tab_names_set (int place, const char *name) {
    if (place < 0 || place >= TABS_PLACES) {
        return;
    }
    load();
    char kept[TAB_NAME_MAX + 1];
    snprintf(kept, sizeof(kept), "%s", name ? name : "");
    /* Spaces at either end are left out; nothing but spaces is no name. */
    char *start = kept;
    while (*start == ' ') {
        start++;
    }
    size_t length = strlen(start);
    while (length > 0 && start[length - 1] == ' ') {
        start[--length] = '\0';
    }
    if (strcmp(names[place], start) == 0) {
        return;
    }
    snprintf(names[place], sizeof(names[0]), "%s", start);
    save();
}
