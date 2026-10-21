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

const char *baked_meta_description (const char *game_code) {
    static char code[5] = "";
    static char *text = NULL;
    if (!game_code) {
        return NULL;
    }
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) game_code[i])) {
            return NULL;
        }
    }
    if (memcmp(code, game_code, 4) == 0) {
        return text;
    }
    free(text);
    memcpy(code, game_code, 4);

    char path[64];
    snprintf(path, sizeof(path), "rom:/meta/%c/%c/%.4s.txt", game_code[1], game_code[2], game_code);
    text = read_text(path);
    if (!text) {
        snprintf(path, sizeof(path), "sd:/menu/metadata/%c/%c/%c/%c/description.txt", game_code[0], game_code[1], game_code[2], game_code[3]);
        text = read_text(path);
    }
    if (!text) {
        snprintf(path, sizeof(path), "sd:/menu/metadata/%c/%c/%c/description.txt", game_code[0], game_code[1], game_code[2]);
        text = read_text(path);
    }
    return text;
}
