/**
 * @file baked_art.c
 * @brief Box art carried inside the menu's own file.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <libdragon.h>

#include "baked_art.h"

#define MAX_LOADED      (8)             /* more than the cover row ever holds at once */
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
    char side = (which == IMAGE_BOXART_BACK) ? 'b' : 'f';
    if (which != IMAGE_BOXART_BACK && which != IMAGE_BOXART_FRONT) {
        return NULL;
    }

    /* The files sit in folders named after the code's two middle letters,
       so no folder holds more than a few dozen entries to look through.
       With the region letter first, then without it. */
    char path[48];
    snprintf(path, sizeof(path), "rom:/art/%c/%c/%c%c%c%c_%c.sprite", game_code[1], game_code[2],
        game_code[0], game_code[1], game_code[2], game_code[3], side);
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
    if (slot < 0 || (heap.total - heap.used) < MIN_FREE) {
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
