#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "card_themes.h"
#include "menu_options.h"
#include "utils/fs.h"

#define NAME_MAX_CHARS  (40)

static char names[CARD_THEMES_MAX][NAME_MAX_CHARS + 1];
static int count = 0;
static bool scanned = false;

static int key_of (const char *name) {
    uint32_t h = 2166136261u;
    for (const char *c = name; *c; c++) {
        h = (h ^ (uint8_t) *c) * 16777619u;
    }
    return (int) (h & 0x7FFFFFFF);
}

static bool theme_file (const char *name, char *out, size_t size) {
    snprintf(out, size, "%s/%s/theme.ini", CARD_THEMES_DIR, name);
    if (file_exists(out)) {
        return true;
    }
    snprintf(out, size, "%s/%s/theme.txt", CARD_THEMES_DIR, name);
    return file_exists(out);
}

static int by_name (const void *a, const void *b) {
    return strcasecmp((const char *) a, (const char *) b);
}

void card_themes_rescan (void) {
    scanned = true;
    count = 0;
    if (!directory_exists(CARD_THEMES_DIR)) {
        return;
    }
    char found[CARD_THEMES_MAX][NAME_MAX_CHARS + 1];
    int n = 0;
    dir_t info;
    int result = dir_findfirst(CARD_THEMES_DIR, &info);
    while (result == 0 && n < CARD_THEMES_MAX) {
        if (info.d_type == DT_DIR && info.d_name[0] != '.' && strlen(info.d_name) <= NAME_MAX_CHARS) {
            snprintf(found[n++], NAME_MAX_CHARS + 1, "%s", info.d_name);
        }
        result = dir_findnext(CARD_THEMES_DIR, &info);
    }
    qsort(found, n, sizeof(found[0]), by_name);
    /* Only folders that hold a theme file. */
    char path[160];
    for (int i = 0; i < n; i++) {
        if (theme_file(found[i], path, sizeof(path))) {
            memcpy(names[count++], found[i], sizeof(names[0]));
        }
    }
    debugf("themes: %d on the card\n", count);
}

static void ensure_scanned (void) {
    if (!scanned) {
        card_themes_rescan();
    }
}

/* The list's place of the theme in use, found again by its folder name. */
static int slot_in_use (void) {
    int choice = options_get(OPTION_THEME);
    if (choice < THEME_CHOICE_CARD) {
        return -1;
    }
    ensure_scanned();
    int key = options_get(OPTION_THEME_KEY);
    for (int i = 0; i < count; i++) {
        if (key_of(names[i]) == key) {
            if (THEME_CHOICE_CARD + i != choice) {
                options_change(OPTION_THEME, THEME_CHOICE_CARD + i);    /* the list moved: follow the folder */
            }
            return i;
        }
    }
    return -1;
}

const char *card_themes_choice_name (int choice) {
    if (choice < THEME_CHOICE_CARD) {
        return builtin_theme_name(choice);
    }
    ensure_scanned();
    if (choice == options_get(OPTION_THEME) && slot_in_use() < 0) {
        return "Midnight (theme gone)";     /* its folder was removed on the PC */
    }
    int i = choice - THEME_CHOICE_CARD;
    return (i < count) ? names[i] : "";
}

void card_themes_note_choice (void) {
    int i = options_get(OPTION_THEME) - THEME_CHOICE_CARD;
    if (i >= 0 && i < count) {
        options_change(OPTION_THEME_KEY, key_of(names[i]));
    }
}

const char *card_themes_path (void) {
    static char path[160];
    int i = slot_in_use();
    if (i < 0 || !theme_file(names[i], path, sizeof(path))) {
        return NULL;
    }
    return path;
}
