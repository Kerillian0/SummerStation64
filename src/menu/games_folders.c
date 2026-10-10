/**
 * @file games_folders.c
 * @brief Which folders the Games tab shows games from.
 */

#include <libdragon.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "games_folders.h"
#include "safe_file.h"

#define FOLDERS_PATH        "sd:/menu/gamefolders.txt"
#define FOLDERS_TMP_PATH    "sd:/menu/gamefolders.tmp"
#define FOLDER_LENGTH       (128)

static char folders[GAMES_FOLDERS_MAX][FOLDER_LENGTH];
static int count = 0;
static bool loaded = false;

/* "sd:/N64(JP)/" -> "/N64(JP)": from the top of the card, no slash at the end. */
static void tidy (char *out, const char *path) {
    const char *from = strstr(path, ":/");
    from = from ? from + 1 : path;
    snprintf(out, FOLDER_LENGTH, "%s", (from[0] == '/') ? from : "/");
    size_t length = strlen(out);
    while (length > 1 && out[length - 1] == '/') {
        out[--length] = '\0';
    }
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;
    count = 0;
    FILE *f = fopen(FOLDERS_PATH, "r");
    if (!f) {
        f = fopen(FOLDERS_TMP_PATH, "r");   /* a save that was cut off */
    }
    if (!f) {
        return;
    }
    char line[FOLDER_LENGTH + 4];
    while (count < GAMES_FOLDERS_MAX && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '/') {
            tidy(folders[count++], line);
        }
    }
    fclose(f);
    debugf("game folders: %d added\n", count);
}

static void save (void) {
    FILE *f = fopen(FOLDERS_TMP_PATH, "w");
    if (!f) {
        debugf("game folders: could not write %s\n", FOLDERS_TMP_PATH);
        return;
    }
    bool ok = true;
    for (int i = 0; i < count; i++) {
        ok = ok && (fprintf(f, "%s\n", folders[i]) > 0);
    }
    ok = (fclose(f) == 0) && ok;
    if (!ok || !safe_file_replace(FOLDERS_TMP_PATH, FOLDERS_PATH)) {
        debugf("game folders: could not save %s\n", FOLDERS_PATH);
    }
}

static int find (const char *tidied) {
    for (int i = 0; i < count; i++) {
        if (!strcasecmp(folders[i], tidied)) {
            return i;
        }
    }
    return -1;
}

int games_folders_list (menu_t *menu, const char **out) {
    load();
    static char start[FOLDER_LENGTH];
    tidy(start, menu->settings.default_directory ? menu->settings.default_directory : "/");
    int n = 0;
    out[n++] = start;
    for (int i = 0; i < count; i++) {
        if (strcasecmp(folders[i], start) != 0) {
            out[n++] = folders[i];
        }
    }
    return n;
}

bool games_folders_has (const char *path) {
    load();
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    return find(tidied) >= 0;
}

bool games_folders_add (const char *path) {
    load();
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    if (find(tidied) >= 0 || count >= GAMES_FOLDERS_MAX) {
        return false;
    }
    strcpy(folders[count++], tidied);
    save();
    return true;
}

bool games_folders_remove (const char *path) {
    load();
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    int i = find(tidied);
    if (i < 0) {
        return false;
    }
    for (; i < count - 1; i++) {
        strcpy(folders[i], folders[i + 1]);
    }
    count--;
    save();
    return true;
}

int games_folders_count (void) {
    load();
    return count;
}

void games_folders_clear (void) {
    load();
    if (count > 0) {
        count = 0;
        save();
    }
}

const char *entry_file_name (const char *name) {
    const char *slash = name ? strrchr(name, '/') : NULL;
    return slash ? slash + 1 : (name ? name : "");
}
