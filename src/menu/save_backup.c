/**
 * @file save_backup.c
 * @brief Keeps spare copies of a game's save file.
 */

#include <fatfs/ff.h>
#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "cart_load.h"
#include "menu_features.h"
#include "path.h"
#include "save_backup.h"
#include "utils/fs.h"

#define BACKUP_FOLDER   "backups"
#define BACKUPS_KEPT    (3)
#define PATH_LENGTH     (320)
#define CHUNK           (4096)

/* Static, so the copy needs no memory of its own (saves are up to 128 KB). */
static uint8_t chunk[CHUNK];
static uint8_t other[CHUNK];

/* "<folder>/backups/<name>.<number>.sav", or with "tmp" for the number 0. */
static void backup_path (char *out, const char *folder, const char *name, int number) {
    if (number > 0) {
        snprintf(out, PATH_LENGTH, "%s/" BACKUP_FOLDER "/%s.%d.sav", folder, name, number);
    } else {
        snprintf(out, PATH_LENGTH, "%s/" BACKUP_FOLDER "/%s.tmp", folder, name);
    }
}

/* Looks through the save: is it blank (every byte the same), and is it the
   same as the newest backup? False if the save can't be read or is empty. */
static bool examine (const char *save, const char *newest, bool *blank, bool *same) {
    FILE *f = fopen(save, "rb");
    if (!f) {
        return false;
    }
    FILE *b = fopen(newest, "rb");
    *blank = true;
    *same = (b != NULL);

    size_t total = 0;
    uint8_t first = 0;
    size_t got;
    while ((got = fread(chunk, 1, CHUNK, f)) > 0) {
        if (total == 0) {
            first = chunk[0];
        }
        for (size_t i = 0; *blank && i < got; i++) {
            if (chunk[i] != first) {
                *blank = false;
            }
        }
        if (*same && (fread(other, 1, got, b) != got || memcmp(chunk, other, got) != 0)) {
            *same = false;
        }
        total += got;
    }
    if (*same && fread(other, 1, 1, b) != 0) {
        *same = false;      /* the backup is longer */
    }

    if (b) {
        fclose(b);
    }
    fclose(f);
    return total > 0;
}

static bool copy (const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    if (!in) {
        return false;
    }
    FILE *out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    bool ok = true;
    size_t got;
    while ((got = fread(chunk, 1, CHUNK, in)) > 0) {
        if (fwrite(chunk, 1, got, out) != got) {
            ok = false;
            break;
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        ok = false;
    }
    return ok;
}

void save_backup_before_launch (menu_t *menu) {
    if (!features_enabled(FEATURE_SAVE_BACKUPS) || !menu->load.rom_path) {
        return;
    }
    uint64_t began = get_ticks_ms();

    /* The save file sits where the loader puts it. */
    path_t *path = path_clone(menu->load.rom_path);
    path_ext_replace(path, "sav");
    if (menu->settings.use_saves_folder) {
        path_push_subdir(path, SAVE_DIRECTORY_NAME);
    }

    static char save[PATH_LENGTH];
    static char folder[PATH_LENGTH];
    static char name[PATH_LENGTH];
    static char target[PATH_LENGTH];
    static char older[PATH_LENGTH];
    snprintf(save, sizeof(save), "%s", path_get(path));
    path_free(path);

    /* Split "sd:/games/saves/Zelda.sav" into its folder and "Zelda". */
    char *slash = strrchr(save, '/');
    if (!slash) {
        return;
    }
    snprintf(folder, sizeof(folder), "%.*s", (int) (slash - save), save);
    snprintf(name, sizeof(name), "%s", slash + 1);
    char *dot = strrchr(name, '.');
    if (dot) {
        *dot = '\0';
    }

    bool blank, same;
    backup_path(target, folder, name, 1);
    if (!examine(save, target, &blank, &same)) {
        return;     /* no save yet: nothing to keep */
    }
    if (blank || same) {
        debugf("save backup: %s, nothing copied (%d ms)\n", blank ? "the save is blank" : "no change since the last copy", (int) (get_ticks_ms() - began));
        return;
    }

    snprintf(older, sizeof(older), "%s/" BACKUP_FOLDER, folder);
    if (!directory_exists(older) && directory_create(older)) {
        debugf("save backup: could not make %s\n", older);
        return;
    }

    /* Copy first, under a temporary name, so a power cut during the copy
       leaves the older backups as they were. */
    backup_path(target, folder, name, 0);
    if (!copy(save, target)) {
        f_unlink(strip_fs_prefix(target));
        debugf("save backup: could not copy to %s\n", target);
        return;
    }

    /* Move the others up one place: 3 goes, 2 becomes 3, 1 becomes 2. */
    backup_path(older, folder, name, BACKUPS_KEPT);
    f_unlink(strip_fs_prefix(older));
    for (int number = BACKUPS_KEPT - 1; number >= 1; number--) {
        static char from[PATH_LENGTH];
        backup_path(from, folder, name, number);
        backup_path(older, folder, name, number + 1);
        f_rename(strip_fs_prefix(from), strip_fs_prefix(older));    /* fails quietly when there is no such copy yet */
    }
    backup_path(older, folder, name, 1);
    if (f_rename(strip_fs_prefix(target), strip_fs_prefix(older)) != FR_OK) {
        debugf("save backup: could not name the copy %s\n", older);
        return;
    }
    debugf("save backup: copied to %s in %d ms\n", older, (int) (get_ticks_ms() - began));
}
