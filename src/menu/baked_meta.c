/**
 * @file baked_meta.c
 * @brief Game details carried inside the menu's own file.
 */

#include <ctype.h>
#include <stdio.h>
#include <libdragon.h>

#include "baked_meta.h"

bool baked_meta_path (const char *game_code, char *path, size_t size) {
    if (!game_code) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) game_code[i])) {
            return false;
        }
    }
    snprintf(path, size, "rom:/meta/%c/%c/%c%c%c%c.ini", game_code[1], game_code[2],
        game_code[0], game_code[1], game_code[2], game_code[3]);
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}
