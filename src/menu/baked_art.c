/**
 * @file baked_art.c
 * @brief Box art carried inside the menu's own file.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "memory_note.h"
#include "utils/fs.h"
#include "baked_art.h"

#define MAX_LOADED      (16)            /* more than the cover row and the selected box's edges ever hold at once */
#define MIN_FREE        (160 * 1024)    /* don't load one when memory is this short */

/* Which covers came from here, and the picture each one's pixels belong to. */
static struct {
    component_boxart_t *art;
    sprite_t *sprite;
} loaded[MAX_LOADED];

static bool code_is_plain (const char *code, int length) {
    for (int i = 0; i < length; i++) {
        if (!isalnum((unsigned char) code[i])) {
            return false;
        }
    }
    return true;
}

/* A European country's letter in a game code. */
static bool is_europe (char letter) {
    return strchr("PDFISUXYHWL", toupper((unsigned char) letter)) != NULL;
}

static bool exists (const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

component_boxart_t *baked_art_load (const char *game_code, file_image_type_t which) {
    if (!game_code || !code_is_plain(game_code, 4)) {
        return NULL;
    }
    char side;
    switch (which) {
        case IMAGE_BOXART_FRONT: side = 'f'; break;
        case IMAGE_BOXART_BACK: side = 'b'; break;
        case IMAGE_BOXART_TOP: side = 't'; break;
        case IMAGE_BOXART_LEFT: side = 'l'; break;
        case IMAGE_BOXART_RIGHT: side = 'r'; break;
        case IMAGE_GAMEPAK_FRONT: side = 'p'; break;
        default: return NULL;
    }

    /* The files sit in folders named after the code's two middle letters,
       so no folder holds more than a few dozen entries to look through.
       With the region letter first, then (in Europe) the European one,
       then without it: a cover without the letter is baked only for codes
       that have no covers for single regions. */
    char path[48];
    snprintf(path, sizeof(path), "rom:/art/%c/%c/%c%c%c%c_%c.sprite", game_code[1], game_code[2],
        game_code[0], game_code[1], game_code[2], game_code[3], side);
    if (!exists(path) && is_europe(game_code[3])) {
        /* Another European country: the pack's European cover. */
        snprintf(path, sizeof(path), "rom:/art/%c/%c/%c%c%cP_%c.sprite", game_code[1], game_code[2],
            game_code[0], game_code[1], game_code[2], side);
    }
    if (!exists(path)) {
        snprintf(path, sizeof(path), "rom:/art/%c/%c/%c%c%c_%c.sprite", game_code[1], game_code[2],
            game_code[0], game_code[1], game_code[2], side);
        if (!exists(path)) {
            return NULL;
        }
    }

    int slot = -1;
    for (int i = 0; i < MAX_LOADED; i++) {
        if (!loaded[i].art) {
            slot = i;
            break;
        }
    }

    /* The loader stops the whole menu if it can't get memory, so make sure
       beforehand that there is room. */
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if (slot < 0) {
        return NULL;
    }
    if ((heap.total - heap.used) < MIN_FREE) {
        memory_note_cut(MEMORY_CUT_BAKED_ART);
        return NULL;
    }

    component_boxart_t *art = calloc(1, sizeof(component_boxart_t));
    surface_t *image = malloc(sizeof(surface_t));
    if (!art || !image) {
        free(art);
        free(image);
        return NULL;
    }

    uint64_t began = get_ticks_ms();
    sprite_t *sprite = sprite_load(path);
    debugf("Baked art: %s in %d ms\n", path + 5, (int) (get_ticks_ms() - began));
    *image = sprite_get_pixels(sprite);
    art->loading = false;
    art->image = image;

    loaded[slot].art = art;
    loaded[slot].sprite = sprite;
    return art;
}

bool baked_art_owns (component_boxart_t *art) {
    for (int i = 0; art && i < MAX_LOADED; i++) {
        if (loaded[i].art == art) {
            return true;
        }
    }
    return false;
}

bool baked_art_free (component_boxart_t *art) {
    if (!art) {
        return false;
    }
    for (int i = 0; i < MAX_LOADED; i++) {
        if (loaded[i].art == art) {
            rspq_wait();    /* nothing may still be drawing from it */
            sprite_free(loaded[i].sprite);
            free(art->image);
            free(art);
            loaded[i].art = NULL;
            loaded[i].sprite = NULL;
            return true;
        }
    }
    return false;
}

bool baked_art_card_ok (const char *storage_prefix, const char *game_code) {
    if (!game_code || (game_code[1] == 'E' && game_code[2] == 'D') || is_europe(game_code[3])) {
        return true;    /* homebrew goes by its title; Europe may use the cover without a letter */
    }
    char folder[64];
    snprintf(folder, sizeof(folder), "%smenu/metadata/%c/%c/%c/%c", storage_prefix, game_code[0], game_code[1], game_code[2], game_code[3]);
    if (directory_exists(folder)) {
        return true;    /* the game has its own */
    }
    folder[strlen(folder) - 2] = '\0';  /* the folder without the region letter */
    if (!directory_exists(folder)) {
        return true;
    }
    /* Covers for single regions beside it: then it is another region's cover. */
    bool ok = true;
    dir_t info;
    int result = dir_findfirst(folder, &info);
    while (result == 0) {
        if (info.d_type == DT_DIR && strlen(info.d_name) == 1) {
            ok = false;
            break;
        }
        result = dir_findnext(folder, &info);
    }
    return ok;
}

component_boxart_t *baked_art_open (const char *storage_prefix, const char *game_code, const char *title, file_image_type_t which) {
    component_boxart_t *art = baked_art_load(game_code, which);
    if (!art && baked_art_card_ok(storage_prefix, game_code)) {
        art = ui_components_boxart_init(storage_prefix, game_code, title, which);
    }
    return art;
}

void baked_art_release (component_boxart_t *art) {
    if (art && !baked_art_free(art)) {
        ui_components_boxart_free(art);
    }
}
