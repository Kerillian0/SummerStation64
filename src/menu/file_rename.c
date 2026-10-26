/**
 * @file file_rename.c
 * @brief Rename a file from the Folders tab, and everything that goes by its name.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fatfs/ff.h>
#include <libdragon.h>

#include "bookkeeping.h"
#include "cart_load.h"
#include "cheat_ready.h"
#include "dd_pair.h"
#include "dump_check.h"
#include "file_rename.h"
#include "game_index.h"
#include "hidden_games.h"
#include "letter_wheel.h"
#include "my_collections.h"
#include "play_stats.h"
#include "retro.h"
#include "sound.h"
#include "utils/fs.h"
#include "views/views.h"

#define PATH_LENGTH     (300)

static bool active = false;
static char old_path[PATH_LENGTH];      /* "sd:/N64/Game.z64" */
static char folder[PATH_LENGTH];        /* "sd:/N64" */
static char old_base[PATH_LENGTH];      /* "Game" */
static char ending[32];                 /* ".z64", or "" */
static int64_t file_size = 0;

/* The endings of files beside a game that go by its name. */
static const char *const beside[] = { ".sav", ".ini", ".meta", ".metadata.ini", ".datel", ".datel.txt" };

static bool rename_on_card (const char *from, const char *to) {
    return f_rename(strip_fs_prefix((char *) from), strip_fs_prefix((char *) to)) == FR_OK;
}

/* One file or folder named after the game, if it is there and the new name is free. */
static void move_along (const char *where, const char *from_name, const char *to_name) {
    char from[PATH_LENGTH], to[PATH_LENGTH];
    snprintf(from, sizeof(from), "%s/%s", where, from_name);
    snprintf(to, sizeof(to), "%s/%s", where, to_name);
    bool there = file_exists(from) || directory_exists(from);
    if (!there || file_exists(to) || directory_exists(to)) {
        return;
    }
    if (rename_on_card(from, to)) {
        debugf("rename: %s -> %s\n", from, to);
    } else {
        debugf("rename: could not move %s\n", from);
    }
}

/* A kept saves folder's name: the save's, without its ending, never ending in a dot or space (save_backup.c). */
static void backup_name (const char *base, char *out, size_t size) {
    snprintf(out, size, "%s", base);
    int n = (int) strlen(out);
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' ')) {
        out[--n] = '\0';
    }
}

static void update_lists (menu_t *menu, const char *new_path) {
    play_stats_rename(old_path, new_path);
    game_index_rename(old_path, new_path, file_size);
    hidden_games_rename(old_path, new_path);
    my_collections_rename_game(old_path, new_path);
    dd_pair_rename(old_path, new_path);
    dump_check_rename(old_path, new_path, (uint32_t) file_size);
    cheat_ready_forget();

    /* Favorites and recent: both paths of each (a 64DD disk keeps its cartridge too). */
    bool changed = false;
    bookkeeping_item_t *lists[2] = { menu->bookkeeping.history_items, menu->bookkeeping.favorite_items };
    int counts[2] = { HISTORY_COUNT, FAVORITES_COUNT };
    for (int l = 0; l < 2; l++) {
        for (int i = 0; i < counts[l]; i++) {
            path_t **paths[2] = { &lists[l][i].primary_path, &lists[l][i].secondary_path };
            for (int p = 0; p < 2; p++) {
                if (*paths[p] && strcmp(path_get(*paths[p]), old_path) == 0) {
                    path_free(*paths[p]);
                    *paths[p] = path_create(new_path);
                    changed = true;
                }
            }
        }
    }
    if (changed) {
        bookkeeping_save(&menu->bookkeeping);
    }
}

static void named (menu_t *menu, const char *name);

static void reopen (const char *prompt, const char *name) {
    letter_wheel_file_name_begin(prompt, name, named);
}

static void named (menu_t *menu, const char *name) {
    if (!name) {
        active = false;     /* given up: nothing changed */
        return;
    }
    /* The card can't end a name in a dot or a space. */
    char base[PATH_LENGTH];
    snprintf(base, sizeof(base), "%s", name);
    int n = (int) strlen(base);
    while (n > 0 && (base[n - 1] == '.' || base[n - 1] == ' ')) {
        base[--n] = '\0';
    }
    if (n == 0) {
        reopen("Type a name", name);
        return;
    }
    if (strcmp(base, old_base) == 0) {
        active = false;     /* the same name: nothing to do */
        return;
    }
    char new_path[PATH_LENGTH];
    snprintf(new_path, sizeof(new_path), "%s/%s%s", folder, base, ending);
    bool case_only = (strcasecmp(base, old_base) == 0);
    if (!case_only && (file_exists(new_path) || directory_exists(new_path))) {
        sound_play_effect(SFX_ERROR);
        reopen("That name is taken", base);     /* nothing touched: choose another */
        return;
    }

    /* The file itself; a change of case only goes through a passing name. */
    bool ok;
    if (case_only) {
        char passing[PATH_LENGTH];
        snprintf(passing, sizeof(passing), "%s/~renaming%s", folder, ending);
        ok = rename_on_card(old_path, passing) && rename_on_card(passing, new_path);
    } else {
        ok = rename_on_card(old_path, new_path);
    }
    if (!ok) {
        active = false;
        menu_show_error(menu, "Couldn't rename the file");
        return;
    }
    debugf("rename: %s -> %s\n", old_path, new_path);

    /* What goes by its name, beside it and in the saves folders. */
    char from_name[PATH_LENGTH], to_name[PATH_LENGTH], where[PATH_LENGTH];
    for (int i = 0; i < (int) (sizeof(beside) / sizeof(beside[0])); i++) {
        snprintf(from_name, sizeof(from_name), "%s%s", old_base, beside[i]);
        snprintf(to_name, sizeof(to_name), "%s%s", base, beside[i]);
        move_along(folder, from_name, to_name);
        snprintf(where, sizeof(where), "%s/" SAVE_DIRECTORY_NAME, folder);
        if (strcmp(beside[i], ".sav") == 0) {
            move_along(where, from_name, to_name);
        }
    }
    char from_backup[PATH_LENGTH], to_backup[PATH_LENGTH];
    backup_name(old_base, from_backup, sizeof(from_backup));
    backup_name(base, to_backup, sizeof(to_backup));
    snprintf(where, sizeof(where), "%s/backups", folder);
    move_along(where, from_backup, to_backup);
    snprintf(where, sizeof(where), "%s/" SAVE_DIRECTORY_NAME "/backups", folder);
    move_along(where, from_backup, to_backup);

    /* A Retro game's art folder is named after the file. */
    const char *system = retro_folder(retro_system(old_path));
    if (system) {
        snprintf(where, sizeof(where), "menu/metadata/%s", system);
        path_t *art = path_init(menu->storage_prefix, where);
        snprintf(where, sizeof(where), "%s", path_get(art));
        path_free(art);
        move_along(where, old_base, base);
    }

    update_lists(menu, new_path);
    active = false;
    sound_play_effect(SFX_SETTING);

    /* The Folders tab reads its folder again, on the renamed file. Made from
       the folder shown, so the path knows where the card's top is (B goes up). */
    char new_file[PATH_LENGTH];
    snprintf(new_file, sizeof(new_file), "%s%s", base, ending);
    menu->browser.select_file = path_clone_push(menu->browser.directory, new_file);
    menu->browser.valid = false;
    view_browser_init(menu);
}

void file_rename_begin (menu_t *menu, const char *path, int64_t size) {
    (void) menu;
    snprintf(old_path, sizeof(old_path), "%s", path);
    file_size = size;
    const char *slash = strrchr(old_path, '/');
    if (!slash) {
        return;
    }
    snprintf(folder, sizeof(folder), "%.*s", (int) (slash - old_path), old_path);
    snprintf(old_base, sizeof(old_base), "%s", slash + 1);
    char *dot = strrchr(old_base, '.');
    ending[0] = '\0';
    if (dot && dot != old_base && strlen(dot) < sizeof(ending)) {
        snprintf(ending, sizeof(ending), "%s", dot);
        *dot = '\0';
    }
    active = true;
    letter_wheel_file_name_begin("Rename", old_base, named);
}

bool file_rename_process (menu_t *menu) {
    if (!active) {
        return false;
    }
    letter_wheel_name_process(menu);
    return true;
}

void file_rename_draw (void) {
    if (active) {
        letter_wheel_draw();
    }
}
