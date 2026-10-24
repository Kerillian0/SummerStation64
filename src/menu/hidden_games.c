#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hidden_games.h"
#include "safe_file.h"

#define HIDDEN_PATH     "sd:/menu/hidden.txt"
#define HIDDEN_TMP_PATH "sd:/menu/hidden.tmp"
#define HIDDEN_HEADER   "hidden 1"

static char *names[HIDDEN_GAMES_MAX];
static int count = 0;
static bool loaded = false;

/* "sd:/N64(US)/Game.z64" -> "N64(US)/Game.z64"; other names as they are. */
static const char *from_top (const char *path) {
    const char *colon = strstr(path, ":/");
    return colon ? colon + 2 : path;
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(HIDDEN_PATH, "r");
    if (!f) {
        f = fopen(HIDDEN_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }
    char line[300];
    bool header = false;
    while (fgets(line, sizeof(line), f) && count < HIDDEN_GAMES_MAX) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!header) {
            header = (strcmp(line, HIDDEN_HEADER) == 0);
            if (!header) {
                break;      /* not a file the menu wrote */
            }
            continue;
        }
        if (line[0]) {
            names[count] = strdup(line);
            count += names[count] ? 1 : 0;
        }
    }
    fclose(f);
    debugf("hidden games: %d\n", count);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (void) {
    FILE *f = fopen(HIDDEN_TMP_PATH, "w");
    if (!f) {
        debugf("hidden games: could not write %s\n", HIDDEN_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", HIDDEN_HEADER);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", names[i]);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(HIDDEN_TMP_PATH, HIDDEN_PATH)) {
        debugf("hidden games: could not replace %s\n", HIDDEN_PATH);
    }
}

static int find (const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

bool hidden_games_name (const char *name) {
    load();
    return count > 0 && name && find(name) >= 0;
}

bool hidden_games_is (const char *path) {
    return path && hidden_games_name(from_top(path));
}

bool hidden_games_set (const char *path, bool hidden) {
    if (!path) {
        return false;
    }
    load();
    const char *name = from_top(path);
    int at = find(name);
    if (hidden && at < 0) {
        if (count >= HIDDEN_GAMES_MAX || !(names[count] = strdup(name))) {
            return false;
        }
        count++;
        debugf("hidden games: hid %s\n", name);
        save();
    } else if (!hidden && at >= 0) {
        free(names[at]);
        memmove(&names[at], &names[at + 1], (count - at - 1) * sizeof(char *));
        count--;
        debugf("hidden games: showing %s again\n", name);
        save();
    }
    return true;
}

int hidden_games_count (void) {
    load();
    return count;
}

void hidden_games_clear (void) {
    load();
    if (count == 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    count = 0;
    save();
    debugf("hidden games: all shown again\n");
}
