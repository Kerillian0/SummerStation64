/**
 * @file cheat_ready.c
 * @brief The cheats a game starts with, when Use Cheats is on.
 */

#include <stdint.h>
#include <libdragon.h>

#include "cheat_ready.h"
#include "datel_codes.h"
#include "path.h"
#include "utils/fs.h"

static uint32_t loaded_for = 0;     /* the game whose codes are in memory, by its path hashed (0: none) */

static uint32_t key_of (const char *path) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) path; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h ? h : 1;
}

void cheat_ready_note (const char *rom_path) {
    loaded_for = key_of(rom_path);
}

void cheat_ready_load (const char *rom_path) {
    if (loaded_for == key_of(rom_path)) {
        return;     /* the editor has them, as the player left them */
    }
    loaded_for = key_of(rom_path);
    path_t *datel = path_create(rom_path);
    path_ext_replace(datel, "datel");
    path_t *datel_txt = path_create(rom_path);
    path_ext_replace(datel_txt, "datel.txt");
    if (file_exists(path_get(datel))) {
        load_cheats_from_file(path_get(datel));
    } else if (file_exists(path_get(datel_txt))) {
        load_cheats_from_file(path_get(datel_txt));
    } else {
        set_cheat_codes(NULL);      /* no file: no codes, never another game's */
        debugf("cheats: no cheat file for %s\n", rom_path);
    }
    path_free(datel);
    path_free(datel_txt);
}
