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

/* Half of a flip: the time to squash the box flat, and again to open it. */
#define FLIP_HALF_MS        (110)

typedef enum {
    FLIP_NONE,      /* at rest */
    FLIP_CLOSING,   /* squashing the side that was showing */
    FLIP_LOADING,   /* flat, waiting for the other side to decode */
    FLIP_OPENING,   /* widening with the new side */
} flip_state_t;

static component_boxart_t *art = NULL;
static entry_t *watched_entry = NULL;
static char *watched_name = NULL;
static bool load_pending = false;
static uint64_t changed_at;

static menu_t *art_menu = NULL;
static file_image_type_t side = IMAGE_BOXART_FRONT;
static flip_state_t flip = FLIP_NONE;
static uint64_t flip_at;

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

    art = ui_components_boxart_init(menu->storage_prefix, (const char *) &header[ROM_CODE_OFFSET], title, side);

    /* No picture of the back for this game: show the front again. */
    if (!art && side != IMAGE_BOXART_FRONT) {
        side = IMAGE_BOXART_FRONT;
        art = ui_components_boxart_init(menu->storage_prefix, (const char *) &header[ROM_CODE_OFFSET], title, side);
    }
}

void carousel_art_reset (void) {
    art_free();
    free(watched_name);
    watched_name = NULL;
    watched_entry = NULL;
    load_pending = false;
    side = IMAGE_BOXART_FRONT;
    flip = FLIP_NONE;
}

static bool art_ready (void) {
    return art && art->image && art->image->width > 0 && art->image->height > 0;
}

bool carousel_art_flip (void) {
    if (flip != FLIP_NONE || !art_ready()) {
        return false;
    }
    flip = FLIP_CLOSING;
    flip_at = get_ticks_ms();
    return true;
}

/* Moves the flip along. With the slide animation switched off the flip is instant too. */
static void flip_update (void) {
    uint64_t half = features_enabled(FEATURE_CAROUSEL_ANIMATION) ? FLIP_HALF_MS : 0;
    uint64_t now = get_ticks_ms();

    if (flip == FLIP_CLOSING && (now - flip_at) >= half) {
        /* Let go of this side before loading the other: one image in memory at a time. */
        art_free();
        side = (side == IMAGE_BOXART_FRONT) ? IMAGE_BOXART_BACK : IMAGE_BOXART_FRONT;
        if (art_menu && watched_entry) {
            art_load(art_menu, watched_entry);
        }
        flip = FLIP_LOADING;
    }

    if (flip == FLIP_LOADING) {
        if (!art || (!art->loading && !art->image)) {
            flip = FLIP_NONE; /* nothing could be loaded: back to the placeholder */
        } else if (art_ready()) {
            flip = FLIP_OPENING;
            flip_at = now;
        }
    }

    if (flip == FLIP_OPENING && (now - flip_at) >= half) {
        flip = FLIP_NONE;
    }
}

float carousel_art_flip_width (void) {
    float t = (float) (get_ticks_ms() - flip_at) / FLIP_HALF_MS;
    if (t > 1.0f) {
        t = 1.0f;
    }
    switch (flip) {
        case FLIP_CLOSING: return 1.0f - t;
        case FLIP_LOADING: return 0.0f;
        case FLIP_OPENING: return t;
        default: return 1.0f;
    }
}

void carousel_art_update (menu_t *menu) {
    if (!features_enabled(FEATURE_COVER_ART)) {
        carousel_art_reset();
        return;
    }

    entry_t *entry = menu->browser.entry;
    art_menu = menu;

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

    flip_update();
}

bool carousel_art_draw (int x0, int y0, int w, int h) {
    if (!art_ready()) {
        /* Mid-flip there is nothing to show, but the placeholder text must stay away. */
        return flip != FLIP_NONE;
    }
    if (w < 2) {
        return true; /* squashed flat */
    }

    /* Fit the art to the cover at its full width, then squash it sideways
       by the same amount the cover itself is squashed during a flip. */
    float squash = carousel_art_flip_width();
    float full_w = (squash > 0.0f) ? (w / squash) : w;
    float fit_x = full_w / art->image->width;
    float fit_y = (float) h / art->image->height;
    float fit = (fit_x < fit_y) ? fit_x : fit_y;
    float scale_x = fit * squash;
    float scale_y = fit;
    int draw_w = (int) (art->image->width * scale_x);
    int draw_h = (int) (art->image->height * scale_y);
    if (draw_w < 1) {
        return true;
    }

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(art->image, x0 + (w - draw_w) / 2, y0 + (h - draw_h) / 2, &(rdpq_blitparms_t) {
            .scale_x = scale_x,
            .scale_y = scale_y,
        });
    rdpq_mode_pop();

    return true;
}
