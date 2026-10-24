/**
 * @file save_backup.c
 * @brief Keeps earlier versions of a game's save file.
 */

#include <fatfs/ff.h>
#include <libdragon.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cart_load.h"
#include "menu_features.h"
#include "path.h"
#include "play_stats.h"
#include "save_backup.h"
#include "sound.h"
#include "utils/fs.h"

#define BACKUP_FOLDER   "backups"
#define ASIDE_NAME      "restore.tmp"   /* a copy on its way back to being the save */
#define COPYING_NAME    "copying.tmp"   /* a copy being written */
#define PATH_LENGTH     (320)
#define CHUNK           (4096)
#define FOUND_MOST      (SAVE_BACKUPS_KEPT + 3)

/* Static, so the copy needs no memory of its own (saves are up to 128 KB). */
static uint8_t chunk[CHUNK];

/* One kept save: "20261013-143500.89abcdef.sav". */
typedef struct {
    char file[40];
    uint64_t when;      /* the date and time as one number, for sorting; 0 if not known */
    uint32_t check;     /* check number of its contents */
    char label[28];     /* "Oct 13, 2:35 PM" */
} kept_t;

static char save[PATH_LENGTH];      /* the game's save file */
static char folder[PATH_LENGTH];    /* this game's folder of kept saves */
static char target[PATH_LENGTH];
static char other[PATH_LENGTH];

static kept_t found[FOUND_MOST];
static int found_count = 0;

static const char *month_names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

bool save_backup_save_path (menu_t *menu, char *out, size_t size) {
    if (!menu->load.rom_path) {
        return false;
    }
    path_t *path = path_clone(menu->load.rom_path);
    path_ext_replace(path, "sav");
    if (menu->settings.use_saves_folder) {
        path_push_subdir(path, SAVE_DIRECTORY_NAME);
    }
    int length = snprintf(out, size, "%s", path_get(path));
    path_free(path);
    return length > 0 && length < (int) size;
}

/* Works out where the game's save and its kept saves are. */
static bool locate (menu_t *menu) {
    if (!menu->load.rom_path) {
        return false;
    }
    /* The save file sits where the loader puts it. */
    path_t *path = path_clone(menu->load.rom_path);
    path_ext_replace(path, "sav");
    if (menu->settings.use_saves_folder) {
        path_push_subdir(path, SAVE_DIRECTORY_NAME);
    }
    snprintf(save, sizeof(save), "%s", path_get(path));
    path_free(path);

    /* "sd:/games/saves/Zelda.sav" -> "sd:/games/saves/backups/Zelda" */
    char *slash = strrchr(save, '/');
    if (!slash) {
        return false;
    }
    int length = snprintf(folder, sizeof(folder), "%.*s/" BACKUP_FOLDER "/%s", (int) (slash - save), save, slash + 1);
    if (length < 0 || length >= (int) sizeof(folder) - 48) {
        return false;   /* no room left for the file names */
    }
    char *dot = strrchr(folder, '.');
    if (dot) {
        *dot = '\0';
    }
    /* The card can't have a folder whose name ends in a dot or a space. */
    length = (int) strlen(folder);
    while (length > 0 && (folder[length - 1] == '.' || folder[length - 1] == ' ')) {
        folder[--length] = '\0';
    }
    return true;
}

/* Reads the save through once: a check number of its contents, and whether
   it is blank (every byte the same). False if it is missing or empty. */
static bool look_at_save (uint32_t *check, bool *blank) {
    FILE *f = fopen(save, "rb");
    if (!f) {
        return false;
    }
    uint32_t h = 2166136261u;
    size_t total = 0;
    uint8_t first = 0;
    *blank = true;
    size_t got;
    while ((got = fread(chunk, 1, CHUNK, f)) > 0) {
        if (total == 0) {
            first = chunk[0];
        }
        for (size_t i = 0; i < got; i++) {
            h = (h ^ chunk[i]) * 16777619u;
            if (chunk[i] != first) {
                *blank = false;
            }
        }
        total += got;
    }
    fclose(f);
    *check = h ^ (uint32_t) total;
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

/* Reads this game's folder of kept saves into `found`, newest first. */
static void find_kept (void) {
    found_count = 0;

    dir_t info;
    int result = dir_findfirst(folder, &info);
    while (result == 0 && found_count < FOUND_MOST) {
        int year, month, day, hour, minute, second;
        unsigned long check;
        char ending[8] = "";
        if (info.d_type != DT_DIR && strlen(info.d_name) < sizeof(found[0].file) &&
            sscanf(info.d_name, "%4d%2d%2d-%2d%2d%2d.%8lx.%3s", &year, &month, &day, &hour, &minute, &second, &check, ending) == 8 &&
            strcmp(ending, "sav") == 0) {
            kept_t *k = &found[found_count++];
            snprintf(k->file, sizeof(k->file), "%s", info.d_name);
            k->check = (uint32_t) check;
            k->when = ((uint64_t) year * 10000000000ull) + ((uint64_t) month * 100000000ull) + ((uint64_t) day * 1000000ull) +
                (hour * 10000) + (minute * 100) + second;
            if (year > 0 && month >= 1 && month <= 12) {
                int hour12 = (hour % 12) ? (hour % 12) : 12;
                snprintf(k->label, sizeof(k->label), "%s %d, %d:%02d %s", month_names[month - 1], day, hour12, minute, (hour >= 12) ? "PM" : "AM");
            } else {
                snprintf(k->label, sizeof(k->label), "Date not known");
            }
        }
        result = dir_findnext(folder, &info);
    }

    for (int i = 1; i < found_count; i++) {     /* newest first */
        kept_t k = found[i];
        int j = i;
        while (j > 0 && found[j - 1].when < k.when) {
            found[j] = found[j - 1];
            j--;
        }
        found[j] = k;
    }
}

/* Keeps the save as it stands, unless it is blank or already kept.
   Call locate() first. */
static void keep_now (menu_t *menu) {
    uint64_t began = get_ticks_ms();

    uint32_t check;
    bool blank;
    if (!look_at_save(&check, &blank)) {
        return;     /* no save yet: nothing to keep */
    }
    if (blank) {
        debugf("save backup: the save is blank, nothing kept (%d ms)\n", (int) (get_ticks_ms() - began));
        return;
    }
    find_kept();
    for (int i = 0; i < found_count; i++) {
        if (found[i].check == check) {
            debugf("save backup: already kept as %s (%d ms)\n", found[i].file, (int) (get_ticks_ms() - began));
            return;
        }
    }

    if (!directory_exists(folder) && directory_create(folder)) {
        debugf("save backup: could not make %s\n", folder);
        return;
    }

    /* Dated by the session the save comes from: the last time the game was
       started. The clock's time now if that isn't known. */
    time_t when = 0;
    play_stats_get(path_get(menu->load.rom_path), &when);
    if (when <= 0) {
        when = menu->current_time;
    }
    char file[40];
    struct tm *t = (when > 0) ? localtime(&when) : NULL;
    if (t) {
        snprintf(file, sizeof(file), "%04d%02d%02d-%02d%02d%02d.%08lx.sav", (t->tm_year + 1900) % 10000, (t->tm_mon + 1) % 100, t->tm_mday % 100,
            t->tm_hour % 100, t->tm_min % 100, t->tm_sec % 100, (unsigned long) check);
    } else {
        snprintf(file, sizeof(file), "00000000-000000.%08lx.sav", (unsigned long) check);
    }

    /* Copy under a working name first, so a power cut during the copy
       leaves no half-written save in the list. */
    snprintf(other, sizeof(other), "%s/" COPYING_NAME, folder);
    snprintf(target, sizeof(target), "%s/%s", folder, file);
    if (!copy(save, other) || f_rename(strip_fs_prefix(other), strip_fs_prefix(target)) != FR_OK) {
        f_unlink(strip_fs_prefix(other));
        debugf("save backup: could not keep %s\n", target);
        return;
    }

    /* More than five: the oldest go, but never the one just kept. */
    int kept = found_count + 1;
    for (int i = found_count - 1; i >= 0 && kept > SAVE_BACKUPS_KEPT; i--) {
        snprintf(other, sizeof(other), "%s/%s", folder, found[i].file);
        f_unlink(strip_fs_prefix(other));
        kept--;
    }
    debugf("save backup: kept as %s in %d ms\n", target, (int) (get_ticks_ms() - began));
}

void save_backup_before_launch (menu_t *menu) {
    if (!features_enabled(FEATURE_SAVE_BACKUPS) || !locate(menu)) {
        return;
    }
    keep_now(menu);
}

/* Going back to a kept save. */

typedef enum {
    PROMPT_NONE,
    PROMPT_ASK,     /* "Go back to the save from ...?" */
    PROMPT_TOLD,    /* what happened; a button closes it */
} prompt_t;

static prompt_t prompt = PROMPT_NONE;
static char prompt_text[200];

static kept_t offered[SAVE_BACKUPS_KEPT];   /* the rows of the submenu */
static int offered_count = 0;
static kept_t chosen;

static void tell (const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(prompt_text, sizeof(prompt_text), fmt, args);
    va_end(args);
    prompt = PROMPT_TOLD;
}

/* A row of the submenu was chosen. */
static void ask (menu_t *menu, void *row) {
    int index = (int) (intptr_t) row;
    if (index < 0 || index >= offered_count || !locate(menu)) {
        tell("No earlier saves of this game are kept yet.\n\nOne is kept each time you start the game\nafter its save has changed.");
        return;
    }
    chosen = offered[index];

    uint32_t check;
    bool blank;
    if (look_at_save(&check, &blank) && check == chosen.check) {
        tell("That is the save you have now.\n\nThere is nothing to go back to.");
        return;
    }
    prompt = PROMPT_ASK;
}

void save_backup_menu_fill (menu_t *menu, component_context_menu_t *list) {
    offered_count = 0;
    if (locate(menu) && directory_exists(folder)) {
        find_kept();
        for (int i = 0; i < found_count && offered_count < SAVE_BACKUPS_KEPT; i++) {
            offered[offered_count++] = found[i];
        }
    }
    for (int i = 0; i < offered_count; i++) {
        list->list[i].text = offered[i].label;
        list->list[i].action = ask;
        list->list[i].arg = (void *) (intptr_t) i;
        list->list[i].submenu = NULL;
    }
    if (offered_count == 0) {
        list->list[0].text = "None kept yet";
        list->list[0].action = ask;
        list->list[0].arg = (void *) (intptr_t) -1;
        list->list[0].submenu = NULL;
    }
    list->list[(offered_count > 0) ? offered_count : 1].text = NULL;    /* the end of the list */
}

/* The chosen save is first copied aside: keeping the present save may push
   the oldest kept one out, which could be the very one asked for. */
static void restore (menu_t *menu) {
    uint64_t began = get_ticks_ms();
    static char from[PATH_LENGTH];
    static char aside[PATH_LENGTH];

    if (!locate(menu)) {
        tell("The save could not be found.");
        return;
    }
    snprintf(from, sizeof(from), "%s/%s", folder, chosen.file);
    snprintf(aside, sizeof(aside), "%s/" ASIDE_NAME, folder);
    if (!copy(from, aside)) {
        f_unlink(strip_fs_prefix(aside));
        tell("That save could not be read.\n\nNothing was changed.");
        return;
    }

    keep_now(menu);     /* the save you have joins the list, so this can be undone */

    f_unlink(strip_fs_prefix(save));
    if (f_rename(strip_fs_prefix(aside), strip_fs_prefix(save)) != FR_OK) {
        debugf("save backup: could not put %s in place of %s\n", aside, save);
        tell("The save could not be put back.\n\nNothing is lost: every kept save\nis still in the list.");
        return;
    }
    debugf("save backup: %s put back as %s in %d ms\n", chosen.file, save, (int) (get_ticks_ms() - began));
    tell("You are back on the save from\n%s.\n\nThe save you had is in the list.", chosen.label);
}

bool save_backup_prompt_process (menu_t *menu) {
    if (prompt == PROMPT_NONE) {
        return false;
    }
    if (prompt == PROMPT_ASK) {
        if (menu->actions.enter) {
            restore(menu);
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            prompt = PROMPT_NONE;
            sound_play_effect(SFX_EXIT);
        }
    } else if (menu->actions.enter || menu->actions.back) {
        prompt = PROMPT_NONE;
        sound_play_effect(SFX_EXIT);
    }
    return true;
}

void save_backup_prompt_draw (void) {
    if (prompt == PROMPT_ASK) {
        ui_components_messagebox_draw(
            "Go back to the save from\n"
            "%s?\n\n"
            "The save you have now is kept too,\n"
            "so you can change your mind.\n\n"
            "A: Go back    B: Cancel",
            chosen.label
        );
    } else if (prompt == PROMPT_TOLD) {
        ui_components_messagebox_draw("%s\n\nA: OK", prompt_text);
    }
}
