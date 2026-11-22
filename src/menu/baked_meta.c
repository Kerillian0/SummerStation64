/**
 * @file baked_meta.c
 * @brief Game details carried inside the menu's own file.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "baked_meta.h"
#include "baked_art.h"

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

#define DESCRIPTION_MOST    (2048)

/* The whole of a small text file, or NULL. */
static char *read_text (const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }
    char *text = malloc(DESCRIPTION_MOST);
    size_t length = text ? fread(text, 1, DESCRIPTION_MOST - 1, f) : 0;
    fclose(f);
    if (!text || length == 0) {
        free(text);
        return NULL;
    }
    text[length] = '\0';
    while (length > 0 && isspace((unsigned char) text[length - 1])) {
        text[--length] = '\0';     /* no blank line at the end of the box */
    }
    return text;
}

/* What the two description lookups last read, kept so they read the card once per game. */
static char own_asked[256] = "";
static char *own_text = NULL;
static char desc_code[5] = "";
static char *desc_text = NULL;

void baked_meta_forget (void) {
    free(own_text);
    own_text = NULL;
    own_asked[0] = '\0';
    free(desc_text);
    desc_text = NULL;
    desc_code[0] = '\0';
}

const char *baked_meta_own_description (const char *rom_path, const char *long_desc_fn) {
    if (!rom_path || !rom_path[0]) {
        return NULL;
    }
    char key[256];
    snprintf(key, sizeof(key), "%s|%s", rom_path, long_desc_fn ? long_desc_fn : "");
    if (strcmp(key, own_asked) == 0) {
        return own_text;    /* this game was looked for already (found or not): no card read every frame */
    }
    free(own_text);
    own_text = NULL;
    snprintf(own_asked, sizeof(own_asked), "%s", key);

    const char *slash = strrchr(rom_path, '/');
    const char *dot = strrchr(rom_path, '.');
    char path[320];
    if (long_desc_fn && long_desc_fn[0]) {
        /* A name in the game's own folder; nothing that climbs out of it. */
        if (strstr(long_desc_fn, "..") || strchr(long_desc_fn, ':') || long_desc_fn[0] == '/' || !slash) {
            debugf("own description: \"%s\" left alone (outside the game's folder)\n", long_desc_fn);
            return NULL;
        }
        snprintf(path, sizeof(path), "%.*s/%s", (int) (slash - rom_path), rom_path, long_desc_fn);
    } else {
        int stem = (dot && (!slash || dot > slash)) ? (int) (dot - rom_path) : (int) strlen(rom_path);
        snprintf(path, sizeof(path), "%.*s.description.txt", stem, rom_path);
    }
    own_text = read_text(path);
    if (own_text) {
        debugf("own description: %s\n", path);
    }
    return own_text;
}

const char *baked_meta_description (const char *game_code) {
    if (!game_code) {
        return NULL;
    }
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) game_code[i])) {
            return NULL;
        }
    }
    if (memcmp(desc_code, game_code, 4) == 0) {
        return desc_text;
    }
    free(desc_text);
    memcpy(desc_code, game_code, 4);

    char path[64];
    snprintf(path, sizeof(path), "rom:/meta/%c/%c/%.4s.txt", game_code[1], game_code[2], game_code);
    desc_text = read_text(path);
    if (!desc_text) {
        snprintf(path, sizeof(path), "sd:/menu/metadata/%c/%c/%c/%c/description.txt", game_code[0], game_code[1], game_code[2], game_code[3]);
        desc_text = read_text(path);
    }
    if (!desc_text && baked_art_card_ok("sd:/", game_code)) {   /* not another region's game sharing the code */
        snprintf(path, sizeof(path), "sd:/menu/metadata/%c/%c/%c/description.txt", game_code[0], game_code[1], game_code[2]);
        desc_text = read_text(path);
    }
    return desc_text;
}
