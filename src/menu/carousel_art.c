#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "carousel_art.h"
#include "baked_art.h"
#include "game_index.h"
#include "cover_list.h"
#include "menu_features.h"
#include "path.h"
#include "ui_components.h"

/* How long the selection must rest before any art is loaded. */
#define SETTLE_TIME_MS      (250)
/* The covers either side wait longer. Starting a load costs one slow frame
   (finding and opening the files on the card), so while the player is
   stepping through the row only the selected cover's art is fetched. */
#define SIDE_SETTLE_TIME_MS (700)
#define INDEX_SAVE_REST_MS  (3000)

/* The selected cover plus two either side. */
#define SLOT_COUNT          (5)

/* Side covers only get art while at least this much memory stays free:
   the 256 KB reserve from the memory budget plus room to decode a picture. */
#define SIDE_ART_MIN_FREE   (384 * 1024)

/* Half of a flip: the time to squash the box flat, and again to open it. */
#define FLIP_HALF_MS        (110)

#define ROM_HEADER_SIZE     (0x40)
#define ROM_TITLE_OFFSET    (0x20)
#define ROM_TITLE_LENGTH    (20)
#define ROM_CODE_OFFSET     (0x3B)

typedef struct {
    bool used;
    int index;                  /* position in the file list */
    entry_t *entry;             /* with the name hash: is it still the same file? */
    uint32_t hash;
    component_boxart_t *art;    /* NULL: this entry has no art, or it wasn't loaded */
} slot_t;

typedef enum {
    FLIP_NONE,      /* at rest */
    FLIP_CLOSING,   /* squashing the side that was showing */
    FLIP_LOADING,   /* flat, waiting for the other side to decode */
    FLIP_OPENING,   /* widening with the new side */
} flip_state_t;

static slot_t slots[SLOT_COUNT];

static menu_t *art_menu = NULL;
static int center_index = -1;
static entry_t *center_entry = NULL;
static uint32_t center_hash = 0;
static uint64_t changed_at;

static file_image_type_t side = IMAGE_BOXART_FRONT;
static flip_state_t flip = FLIP_NONE;
static uint64_t flip_at;

/* ---------- small helpers ---------- */

static uint32_t name_hash (const char *name) {
    uint32_t hash = 2166136261u;
    while (*name) {
        hash = (hash ^ (uint8_t) *name++) * 16777619u;
    }
    return hash;
}

static bool art_ready (component_boxart_t *art) {
    return art && art->image && art->image->width > 0 && art->image->height > 0;
}

static void slot_clear (slot_t *slot) {
    if (slot->art) {
        if (!baked_art_free(slot->art)) ui_components_boxart_free(slot->art); /* also stops a decode in progress */
    }
    memset(slot, 0, sizeof(*slot));
}

/* The slot holding list entry `index`, if it is still the same file. */
static slot_t *slot_find (menu_t *menu, int index) {
    if (index < 0 || index >= cover_list_current(menu)->entries) {
        return NULL;
    }
    entry_t *entry = &cover_list_current(menu)->list[index];
    for (int i = 0; i < SLOT_COUNT; i++) {
        slot_t *slot = &slots[i];
        if (slot->used && slot->index == index && slot->entry == entry && slot->hash == name_hash(entry->name)) {
            return slot;
        }
    }
    return NULL;
}

static slot_t *slot_free (void) {
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (!slots[i].used) {
            return &slots[i];
        }
    }
    return NULL;
}

static bool any_loading (void) {
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (slots[i].used && slots[i].art && slots[i].art->loading) {
            return true;
        }
    }
    return false;
}

static bool memory_for_side_art (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    return (heap.total - heap.used) >= SIDE_ART_MIN_FREE;
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

/* Start decoding the art for an entry. Returns NULL if it has none. */
static component_boxart_t *art_load (menu_t *menu, int index, file_image_type_t *which) {
    char code[5] = "";
    char title[ROM_TITLE_LENGTH + 1] = "";

    const cover_list_t *covers = cover_list_current(menu);
    path_t *path = covers->path(menu, index);
    int64_t size = covers->list[index].size;

    /* A game seen before needs no look at its file: the list knows its code. */
    const game_index_entry_t *known = game_index_find(path_get(path), size);
    if (known) {
        memcpy(code, known->code, 4);
    } else {
        uint8_t header[ROM_HEADER_SIZE];

        FILE *f = fopen(path_get(path), "rb");
        if (!f) {
            path_free(path);
            return NULL;
        }
        size_t read = fread(header, 1, sizeof(header), f);
        fclose(f);
        if (read != sizeof(header)) {
            path_free(path);
            return NULL;
        }

        fix_header_byte_order(header);
        memcpy(title, &header[ROM_TITLE_OFFSET], ROM_TITLE_LENGTH);
        memcpy(code, &header[ROM_CODE_OFFSET], 4);

        /* Homebrew is found by its title instead of a code, so it is not noted. */
        if (!(code[1] == 'E' && code[2] == 'D')) {
            game_index_set_code(path_get(path), size, code);
        }
    }
    path_free(path);

    /* Art baked into the menu comes first: it needs no search of the card
       and no unpacking. */
    component_boxart_t *art = baked_art_load(code, *which);
    if (art) {
        return art;
    }

    art = ui_components_boxart_init(menu->storage_prefix, code, title, *which);

    /* No picture of the back for this game: show the front again. */
    if (!art && *which != IMAGE_BOXART_FRONT) {
        *which = IMAGE_BOXART_FRONT;
        art = baked_art_load(code, *which);
        if (!art) {
            art = ui_components_boxart_init(menu->storage_prefix, code, title, *which);
        }
    }

    return art;
}

/* ---------- flipping the selected box ---------- */

bool carousel_art_flip (void) {
    if (flip != FLIP_NONE || !art_menu) {
        return false;
    }
    slot_t *center = slot_find(art_menu, center_index);
    if (!center || !art_ready(center->art)) {
        return false;
    }
    flip = FLIP_CLOSING;
    flip_at = get_ticks_ms();
    return true;
}

/* Moves the flip along. With the slide animation switched off the flip is instant too. */
static void flip_update (menu_t *menu) {
    if (flip == FLIP_NONE) {
        return;
    }

    uint64_t half = features_enabled(FEATURE_CAROUSEL_ANIMATION) ? FLIP_HALF_MS : 0;
    uint64_t now = get_ticks_ms();
    slot_t *center = slot_find(menu, center_index);

    if (!center) {
        flip = FLIP_NONE;
        return;
    }

    if (flip == FLIP_CLOSING && (now - flip_at) >= half) {
        /* The decoder handles one picture at a time: a side cover that is
           still loading has to wait (it is picked up again afterwards). */
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (&slots[i] != center && slots[i].used && slots[i].art && slots[i].art->loading) {
                slot_clear(&slots[i]);
            }
        }
        /* Let go of this side before loading the other: no extra memory. */
        if (center->art) {
            if (!baked_art_free(center->art)) ui_components_boxart_free(center->art);
        }
        side = (side == IMAGE_BOXART_FRONT) ? IMAGE_BOXART_BACK : IMAGE_BOXART_FRONT;
        center->art = art_load(menu, center->index, &side);
        flip = FLIP_LOADING;
    }

    if (flip == FLIP_LOADING) {
        if (!center->art || (!center->art->loading && !center->art->image)) {
            flip = FLIP_NONE; /* nothing could be loaded: back to the placeholder */
        } else if (art_ready(center->art)) {
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

/* ---------- the cache ---------- */

void carousel_art_reset (void) {
    for (int i = 0; i < SLOT_COUNT; i++) {
        slot_clear(&slots[i]);
    }
    center_index = -1;
    center_entry = NULL;
    center_hash = 0;
    side = IMAGE_BOXART_FRONT;
    flip = FLIP_NONE;
    game_index_flush();     /* leaving the covers: save what was learned about the games */
}

void carousel_art_update (menu_t *menu, bool side_covers) {
    art_menu = menu;

    if (!features_enabled(FEATURE_COVER_ART) || cover_list_current(menu)->entries <= 0 || cover_list_current(menu)->selected < 0) {
        carousel_art_reset();
        return;
    }

    uint64_t now = get_ticks_ms();
    int selected = cover_list_current(menu)->selected;
    entry_t *entry = &cover_list_current(menu)->list[selected];
    uint32_t hash = name_hash(entry->name);

    if (selected != center_index || entry != center_entry || hash != center_hash) {
        /* A box left showing its back would turn up as a side cover that way round. */
        if (side != IMAGE_BOXART_FRONT) {
            for (int i = 0; i < SLOT_COUNT; i++) {
                if (slots[i].used && slots[i].index == center_index && slots[i].entry == center_entry) {
                    slot_clear(&slots[i]);
                }
            }
        }
        side = IMAGE_BOXART_FRONT;
        flip = FLIP_NONE;
        center_index = selected;
        center_entry = entry;
        center_hash = hash;
        changed_at = now;
    }

    /* The covers we want art for, most important first. */
    static const int order[SLOT_COUNT] = { 0, 1, -1, 2, -2 };
    int wanted[SLOT_COUNT];
    int wanted_count = 0;
    for (int i = 0; i < (side_covers ? SLOT_COUNT : 1); i++) {
        int index = selected + order[i];
        if (index >= 0 && index < cover_list_current(menu)->entries) {
            wanted[wanted_count++] = index;
        }
    }

    /* Let go of everything that has scrolled out of view (or belongs to another folder). */
    for (int i = 0; i < SLOT_COUNT; i++) {
        slot_t *slot = &slots[i];
        if (!slot->used) {
            continue;
        }
        bool keep = false;
        for (int j = 0; j < wanted_count; j++) {
            if (slot_find(menu, wanted[j]) == slot) {
                keep = true;
                break;
            }
        }
        if (!keep) {
            slot_clear(slot);
        }
    }

    /* Start at most one new decode, once the selection has settled. */
    if (flip == FLIP_NONE && !any_loading() && (now - changed_at) >= SETTLE_TIME_MS) {
        for (int j = 0; j < wanted_count; j++) {
            int index = wanted[j];
            if (slot_find(menu, index)) {
                continue; /* already loaded, or already known to have no art */
            }
            if (index != selected && (now - changed_at) < SIDE_SETTLE_TIME_MS) {
                break; /* the side covers wait until the selection has really come to rest */
            }
            slot_t *slot = slot_free();
            if (!slot) {
                break;
            }
            entry_t *e = &cover_list_current(menu)->list[index];
            slot->used = true;
            slot->index = index;
            slot->entry = e;
            slot->hash = name_hash(e->name);
            slot->art = NULL;
            if (e->type == ENTRY_TYPE_ROM && (index == selected || memory_for_side_art())) {
                file_image_type_t which = IMAGE_BOXART_FRONT;
                uint64_t began = get_ticks_ms();
                slot->art = art_load(menu, index, &which);
                debugf("cover: starting one took %d ms\n", (int) (get_ticks_ms() - began));
            }
            if (slot->art) {
                break;
            }
        }
    }

    flip_update(menu);

    /* Also saved once the selection has rested a while, in case the console
       is switched off without leaving this screen. */
    if ((now - changed_at) >= INDEX_SAVE_REST_MS) {
        game_index_flush();
    }
}

bool carousel_art_draw (menu_t *menu, int index, int x0, int y0, int w, int h, int alpha, bool center) {
    slot_t *slot = slot_find(menu, index);
    bool flipping = center && (flip != FLIP_NONE);

    if (!slot || !art_ready(slot->art)) {
        /* Mid-flip there is nothing to show, but the placeholder text must stay away. */
        return flipping;
    }
    if (w < 2) {
        return true; /* squashed flat */
    }

    surface_t *image = slot->art->image;

    /* Fit the art to the cover at its full width, then squash it sideways
       by the same amount the cover itself is squashed during a flip. */
    float squash = flipping ? carousel_art_flip_width() : 1.0f;
    float full_w = (squash > 0.0f) ? (w / squash) : w;
    float fit_x = full_w / image->width;
    float fit_y = (float) h / image->height;
    float fit = (fit_x < fit_y) ? fit_x : fit_y;
    float scale_x = fit * squash;
    float scale_y = fit;
    int draw_w = (int) (image->width * scale_x);
    int draw_h = (int) (image->height * scale_y);
    if (draw_w < 1) {
        return true;
    }

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        if (alpha >= 255) {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        } else {
            /* See-through, like the placeholder covers at the sides. */
            rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, alpha));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        rdpq_tex_blit(image, x0 + (w - draw_w) / 2, y0 + (h - draw_h) / 2, &(rdpq_blitparms_t) {
            .scale_x = scale_x,
            .scale_y = scale_y,
        });
    rdpq_mode_pop();

    return true;
}
