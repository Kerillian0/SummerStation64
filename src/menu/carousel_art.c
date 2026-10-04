#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "carousel_art.h"
#include "menu_features.h"
#include "path.h"
#include "ui_components.h"

/* How long the selection must rest before its art is loaded. */
#define SETTLE_TIME_MS      (250)

#define ROM_HEADER_SIZE     (0x40)
#define ROM_TITLE_OFFSET    (0x20)
#define ROM_TITLE_LENGTH    (20)
#define ROM_CODE_OFFSET     (0x3B)

static component_boxart_t *art = NULL;
static entry_t *watched_entry = NULL;
static char *watched_name = NULL;
static bool load_pending = false;
static uint64_t changed_at;

static void art_free (void) {
    if (art) {
        ui_components_boxart_free(art);
        art = NULL;
    }
}

/* ROM dumps come in three byte orders; put the header back in the native one. */
static void fix_header_byte_order (uint8_t *h) {
    if (h[0] == 0x37 && h[1] == 0x80) {         /* byte swapped (.v64) */
        for (int i = 0; i < ROM_HEADER_SIZE; i += 2) {
            uint8_t t = h[i]; h[i] = h[i + 1]; h[i + 1] = t;
        }
    } else if (h[0] == 0x40 && h[1] == 0x12) {  /* little endian (.n64) */
        for (int i = 0; i < ROM_HEADER_SIZE; i += 4) {
            uint8_t t = h[i]; h[i] = h[i + 3]; h[i + 3] = t;
            t = h[i + 1]; h[i + 1] = h[i + 2]; h[i + 2] = t;
        }
    }
}

static void art_load (menu_t *menu, entry_t *entry) {
    uint8_t header[ROM_HEADER_SIZE];

    path_t *path = path_clone_push(menu->browser.directory, entry->name);
    FILE *f = fopen(path_get(path), "rb");
    path_free(path);
    if (!f) {
        return;
    }
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);
    if (read != sizeof(header)) {
        return;
    }

    fix_header_byte_order(header);

    char title[ROM_TITLE_LENGTH + 1];
    memcpy(title, &header[ROM_TITLE_OFFSET], ROM_TITLE_LENGTH);
    title[ROM_TITLE_LENGTH] = '\0';

    art = ui_components_boxart_init(menu->storage_prefix, (const char *) &header[ROM_CODE_OFFSET], title, IMAGE_BOXART_FRONT);
}

void carousel_art_reset (void) {
    art_free();
    free(watched_name);
    watched_name = NULL;
    watched_entry = NULL;
    load_pending = false;
}

void carousel_art_update (menu_t *menu) {
    if (!features_enabled(FEATURE_COVER_ART)) {
        carousel_art_reset();
        return;
    }

    entry_t *entry = menu->browser.entry;

    bool changed = (entry != watched_entry) ||
        (entry && (!watched_name || strcmp(entry->name, watched_name) != 0));

    if (changed) {
        carousel_art_reset();
        watched_entry = entry;
        watched_name = entry ? strdup(entry->name) : NULL;
        load_pending = entry && (entry->type == ENTRY_TYPE_ROM);
        changed_at = get_ticks_ms();
    }

    if (load_pending && (get_ticks_ms() - changed_at) >= SETTLE_TIME_MS) {
        load_pending = false;
        art_load(menu, entry);
    }
}

bool carousel_art_draw (int x0, int y0, int w, int h) {
    if (!art || !art->image || art->image->width == 0 || art->image->height == 0) {
        return false;
    }

    float scale_x = (float) w / art->image->width;
    float scale_y = (float) h / art->image->height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int draw_w = (int) (art->image->width * scale);
    int draw_h = (int) (art->image->height * scale);

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(art->image, x0 + (w - draw_w) / 2, y0 + (h - draw_h) / 2, &(rdpq_blitparms_t) {
            .scale_x = scale,
            .scale_y = scale,
        });
    rdpq_mode_pop();

    return true;
}
