#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "homebrew_marks.h"
#include "safe_file.h"

#define MARKS_PATH      "sd:/menu/homebrew.txt"     /* every profile's: what a game is doesn't change with the player */
#define MARKS_TMP_PATH  "sd:/menu/homebrew.tmp"
#define MARKS_HEADER    "homebrew 1"

static char *names[HOMEBREW_MARKS_MAX];
static int8_t marks[HOMEBREW_MARKS_MAX];
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
    FILE *f = fopen(MARKS_PATH, "r");
    if (!f) {
        f = fopen(MARKS_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }
    char line[300];
    bool header = false;
    while (fgets(line, sizeof(line), f) && count < HOMEBREW_MARKS_MAX) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!header) {
            header = (strcmp(line, MARKS_HEADER) == 0);
            if (!header) {
                break;      /* not a file the menu wrote */
            }
            continue;
        }
        if ((line[0] == '+' || line[0] == '-') && line[1] == ' ' && line[2]) {
            names[count] = strdup(line + 2);
            if (names[count]) {
                marks[count++] = (line[0] == '+') ? 1 : -1;
            }
        }
    }
    fclose(f);
    debugf("homebrew marks: %d\n", count);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (void) {
    FILE *f = fopen(MARKS_TMP_PATH, "w");
    if (!f) {
        debugf("homebrew marks: could not write %s\n", MARKS_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", MARKS_HEADER);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%c %s\n", (marks[i] > 0) ? '+' : '-', names[i]);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(MARKS_TMP_PATH, MARKS_PATH)) {
        debugf("homebrew marks: could not replace %s\n", MARKS_PATH);
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

int homebrew_marks_get (const char *name) {
    load();
    int at = (count > 0 && name) ? find(name) : -1;
    return (at >= 0) ? marks[at] : 0;
}

int homebrew_marks_get_path (const char *path) {
    return path ? homebrew_marks_get(from_top(path)) : 0;
}

bool homebrew_marks_set (const char *path, int mark) {
    if (!path) {
        return false;
    }
    load();
    const char *name = from_top(path);
    int at = find(name);
    if (mark == 0) {
        if (at >= 0) {
            free(names[at]);
            memmove(&names[at], &names[at + 1], (count - at - 1) * sizeof(char *));
            memmove(&marks[at], &marks[at + 1], (count - at - 1) * sizeof(int8_t));
            count--;
            save();
        }
        return true;
    }
    if (at < 0) {
        if (count >= HOMEBREW_MARKS_MAX || !(names[count] = strdup(name))) {
            return false;
        }
        at = count++;
    } else if (marks[at] == mark) {
        return true;
    }
    marks[at] = (mark > 0) ? 1 : -1;
    debugf("homebrew marks: %s %s\n", name, (mark > 0) ? "is homebrew" : "is not homebrew");
    save();
    return true;
}

void homebrew_marks_rename (const char *old_path, const char *new_path) {
    load();
    int at = find(from_top(old_path));
    if (at >= 0) {
        char *copy = strdup(from_top(new_path));
        if (copy) {
            free(names[at]);
            names[at] = copy;
            save();
        }
    }
}
