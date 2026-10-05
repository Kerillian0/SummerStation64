#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "folder_memory.h"
#include "ini_parser.h"
#include "menu_features.h"
#include "path.h"

#define FOLDER_MEMORY_PATH      "sd:/menu/folders.ini"
#define FOLDER_MEMORY_TMP_PATH  "sd:/menu/folders.tmp"

#define MAX_FOLDERS     (16)    /* most recently used first */
#define MAX_TEXT        (256)

typedef struct {
    char path[MAX_TEXT];
    char name[MAX_TEXT];
} folder_t;

static folder_t folders[MAX_FOLDERS];
static int folder_count = 0;
static bool loaded = false;
static bool dirty = false;

/* The folder being shown, and the selection last recorded for it. */
static char current_path[MAX_TEXT] = "";
static int current_selected = -1;

static void copy_text (char *dst, const char *src) {
    strncpy(dst, src, MAX_TEXT - 1);
    dst[MAX_TEXT - 1] = '\0';
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    ini_t *ini = ini_load(FOLDER_MEMORY_PATH);
    if (!ini) {
        ini = ini_try_load(FOLDER_MEMORY_TMP_PATH);
    }

    for (int i = 0; i < MAX_FOLDERS; i++) {
        char section[16];
        snprintf(section, sizeof(section), "folder%d", i);
        const char *path = ini_get_string(ini, section, "path", "");
        const char *name = ini_get_string(ini, section, "selected", "");
        if (path[0] != '\0' && name[0] != '\0') {
            copy_text(folders[folder_count].path, path);
            copy_text(folders[folder_count].name, name);
            folder_count++;
        }
    }

    ini_free(ini);
}

static const char *lookup (const char *path) {
    for (int i = 0; i < folder_count; i++) {
        if (strcmp(folders[i].path, path) == 0) {
            return folders[i].name;
        }
    }
    return NULL;
}

/* Record the selection for a folder and move that folder to the front. */
static void remember (const char *path, const char *name) {
    int found = -1;
    for (int i = 0; i < folder_count; i++) {
        if (strcmp(folders[i].path, path) == 0) {
            found = i;
            break;
        }
    }

    if (found == 0) {
        if (strcmp(folders[0].name, name) != 0) {
            copy_text(folders[0].name, name);
            dirty = true;
        }
        return;
    }

    /* Make room at the front; the oldest folder drops off the end if the list is full. */
    int last = (found > 0) ? found : ((folder_count < MAX_FOLDERS) ? folder_count++ : MAX_FOLDERS - 1);
    memmove(&folders[1], &folders[0], last * sizeof(folder_t));
    copy_text(folders[0].path, path);
    copy_text(folders[0].name, name);
    dirty = true;
}

void folder_memory_update (menu_t *menu) {
    if (!features_enabled(FEATURE_REMEMBER_SELECTION) || !menu->browser.directory) {
        return;
    }

    load();

    const char *path = path_get(menu->browser.directory);

    if (strcmp(path, current_path) != 0) {
        /* Just arrived in another folder. Only move the selection if nothing
           else has placed it (going back up already selects the folder left). */
        copy_text(current_path, path);
        current_selected = -1;

        const char *name = lookup(path);
        if (name && menu->browser.selected == 0) {
            for (int i = 0; i < menu->browser.entries; i++) {
                if (strcmp(menu->browser.list[i].name, name) == 0) {
                    menu->browser.selected = i;
                    menu->browser.entry = &menu->browser.list[i];
                    break;
                }
            }
        }
    }

    if (menu->browser.entry && menu->browser.selected != current_selected) {
        current_selected = menu->browser.selected;
        remember(path, menu->browser.entry->name);
    }
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
void folder_memory_flush (void) {
    if (!dirty) {
        return;
    }
    dirty = false;

    ini_t *ini = ini_create();
    for (int i = 0; i < folder_count; i++) {
        char section[16];
        snprintf(section, sizeof(section), "folder%d", i);
        ini_set_string(ini, section, "path", folders[i].path);
        ini_set_string(ini, section, "selected", folders[i].name);
    }

    if (ini_save(ini, FOLDER_MEMORY_TMP_PATH)) {
        remove(FOLDER_MEMORY_PATH);
        if (rename(FOLDER_MEMORY_TMP_PATH, FOLDER_MEMORY_PATH) != 0) {
            debugf("folder memory: could not replace %s\n", FOLDER_MEMORY_PATH);
        }
    } else {
        debugf("folder memory: could not write %s\n", FOLDER_MEMORY_TMP_PATH);
    }

    ini_free(ini);
}
