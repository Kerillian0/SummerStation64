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
#define HACKS_PATH          "sd:/menu/hackfolders.txt"
#define HACKS_TMP_PATH      "sd:/menu/hackfolders.tmp"
#define FOLDER_LENGTH       (128)

/* A list of folders kept in a file on the card, one per line. */
typedef struct {
    const char *path, *tmp_path, *what;
    char folders[GAMES_FOLDERS_MAX + 1][FOLDER_LENGTH];
    int count;
    bool loaded;
} folder_list_t;

static folder_list_t games = { .path = FOLDERS_PATH, .tmp_path = FOLDERS_TMP_PATH, .what = "game folders" };
static folder_list_t hacks = { .path = HACKS_PATH, .tmp_path = HACKS_TMP_PATH, .what = "hack folders" };

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

static void load (folder_list_t *list) {
    if (list->loaded) {
        return;
    }
    list->loaded = true;
    list->count = 0;
    FILE *f = fopen(list->path, "r");
    if (!f) {
        f = fopen(list->tmp_path, "r");     /* a save that was cut off */
    }
    if (!f) {
        return;
    }
    char line[FOLDER_LENGTH + 4];
    int most = (list == &hacks) ? GAMES_FOLDERS_MAX + 1 : GAMES_FOLDERS_MAX;
    while (list->count < most && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '/') {
            tidy(list->folders[list->count++], line);
        }
    }
    fclose(f);
    debugf("%s: %d added\n", list->what, list->count);
}

static void save (folder_list_t *list) {
    FILE *f = fopen(list->tmp_path, "w");
    if (!f) {
        debugf("%s: could not write %s\n", list->what, list->tmp_path);
        return;
    }
    bool ok = true;
    for (int i = 0; i < list->count; i++) {
        ok = ok && (fprintf(f, "%s\n", list->folders[i]) > 0);
    }
    ok = (fclose(f) == 0) && ok;
    if (!ok || !safe_file_replace(list->tmp_path, list->path)) {
        debugf("%s: could not save %s\n", list->what, list->path);
    }
}

static int find (folder_list_t *list, const char *tidied) {
    for (int i = 0; i < list->count; i++) {
        if (!strcasecmp(list->folders[i], tidied)) {
            return i;
        }
    }
    return -1;
}

static bool list_add (folder_list_t *list, const char *path, int most) {
    load(list);
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    if (find(list, tidied) >= 0 || list->count >= most) {
        return false;
    }
    strcpy(list->folders[list->count++], tidied);
    save(list);
    return true;
}

static bool list_remove (folder_list_t *list, const char *path) {
    load(list);
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    int i = find(list, tidied);
    if (i < 0) {
        return false;
    }
    for (; i < list->count - 1; i++) {
        strcpy(list->folders[i], list->folders[i + 1]);
    }
    list->count--;
    save(list);
    return true;
}

int games_folders_list (menu_t *menu, const char **out) {
    load(&games);
    static char start[FOLDER_LENGTH];
    tidy(start, menu->settings.default_directory ? menu->settings.default_directory : "/");
    int n = 0;
    out[n++] = start;
    for (int i = 0; i < games.count; i++) {
        if (strcasecmp(games.folders[i], start) != 0) {
            out[n++] = games.folders[i];
        }
    }
    return n;
}

bool games_folders_has (const char *path) {
    load(&games);
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    return find(&games, tidied) >= 0;
}

bool games_folders_add (const char *path) {
    return list_add(&games, path, GAMES_FOLDERS_MAX);
}

bool games_folders_remove (const char *path) {
    list_remove(&hacks, path);  /* no longer in the Games tab: no longer marked either */
    return list_remove(&games, path);
}

int games_folders_count (void) {
    load(&games);
    return games.count;
}

void games_folders_clear (void) {
    load(&games);
    if (games.count > 0) {
        games.count = 0;
        save(&games);
    }
    load(&hacks);
    if (hacks.count > 0) {
        hacks.count = 0;
        save(&hacks);
    }
}

bool games_folders_is_hacks (const char *path) {
    load(&hacks);
    char tidied[FOLDER_LENGTH];
    tidy(tidied, path);
    return find(&hacks, tidied) >= 0;
}

bool games_folders_mark_hacks (const char *path, bool marked) {
    return marked ? list_add(&hacks, path, GAMES_FOLDERS_MAX + 1) : list_remove(&hacks, path);
}

bool games_folders_entry_in_hacks (const char *name) {
    load(&hacks);
    if (!name || hacks.count == 0) {
        return false;
    }
    /* "N64 Hacks/Game.z64" -> "/N64 Hacks"; a name without a folder is in the top of the card. */
    char folder[FOLDER_LENGTH];
    const char *slash = strrchr(name, '/');
    int length = slash ? (int) (slash - name) : 0;
    if (length >= FOLDER_LENGTH - 2) {
        return false;
    }
    folder[0] = '/';
    memcpy(folder + 1, name, length);
    folder[length + 1] = '\0';
    if (length == 0) {
        folder[1] = '\0';
    }
    return find(&hacks, folder) >= 0;
}

const char *entry_file_name (const char *name) {
    const char *slash = name ? strrchr(name, '/') : NULL;
    return slash ? slash + 1 : (name ? name : "");
}
