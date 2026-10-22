#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory_note.h"
#include "carousel_art.h"
#include "art_tint.h"
#include "game_learn.h"
#include "baked_art.h"
#include "box3d.h"
#include "box_motion.h"
#include "theme.h"
#include "game_index.h"
#include "cover_list.h"
#include "menu_features.h"
#include "path.h"
#include "safe_file.h"
#include "ui_components.h"

/* How long the selection must rest before any art is loaded. */
#define SETTLE_TIME_MS      (250)
/* Art baked into the menu loads in a few milliseconds, so it need not wait
   as long as art read from the card. These apply while the covers being
   shown are coming from baked art. */
#define BAKED_SETTLE_TIME_MS        (100)
#define BAKED_SIDE_SETTLE_TIME_MS   (250)
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

#define ROM_TITLE_LENGTH    (GAME_INDEX_TITLE_LENGTH)

typedef struct {
    bool used;
    int index;                  /* position in the file list */
    entry_t *entry;             /* with the name hash: is it still the same file? */
    uint32_t hash;
    component_boxart_t *art;    /* NULL: this entry has no art, or it wasn't loaded */
    bool card_pending;          /* no baked art; its picture on the card is still to be tried, once the decoder is free */
    uint8_t tint_state;         /* ring color: 0 not worked out yet, 1 the cover has none, 2 in `tint` */
    color_t tint;
    component_boxart_t *top;    /* the box's top label, for 3D Side Covers (baked art only) */
    bool top_tried;
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

/* The selected box's edges for the 3D Box: top strip, left and right side.
   Fetched one at a time once its cover is in, and let go when the
   selection moves. */
#define EDGE_COUNT          (3)
#define BOX_EASE_MS         (250)   /* how long a flat cover takes to become a box */
#define HOVER_EASE_MS       (500)   /* how long the hover takes to start once the box has come to rest */
#define BOX_YAW             (CAROUSEL_BOX_YAW) /* degrees turned, showing the left side */
#define BOX_PITCH           (14.0f) /* degrees seen from above, showing the top */
static const file_image_type_t edge_kinds[EDGE_COUNT] = { IMAGE_BOXART_TOP, IMAGE_BOXART_LEFT, IMAGE_BOXART_RIGHT };
static component_boxart_t *edges[EDGE_COUNT];
static bool edge_tried[EDGE_COUNT];
static uint64_t box_since = 0;      /* when the box started to open out (0: not yet) */
static bool box_from_flat = false;  /* it was flat (turned over): open out again even with 3D Side Covers */

static int art_bright = 255;      /* see carousel_art_brightness() */

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
    if (slot->top) {
        baked_art_free(slot->top);
    }
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
    for (int i = 0; i < EDGE_COUNT; i++) {
        if (edges[i] && edges[i]->loading) {
            return true;
        }
    }
    return false;
}

static void edges_clear (void) {
    for (int i = 0; i < EDGE_COUNT; i++) {
        if (edges[i] && !baked_art_free(edges[i])) {
            ui_components_boxart_free(edges[i]);    /* also stops a decode in progress */
        }
        edges[i] = NULL;
        edge_tried[i] = false;
    }
    box_since = 0;
}

static bool memory_for_side_art (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if ((heap.total - heap.used) < SIDE_ART_MIN_FREE) {
        memory_note_cut(MEMORY_CUT_SIDE_ART);
        return false;
    }
    return true;
}

/* This menu carries baked art (one cover has come from it), so covers use
   the short waits. It stays set when a game without baked art goes by: one
   such game used to put the covers after it back on the long waits. */
static bool baked_lately = false;

/* Start decoding the art for an entry. Returns NULL if it has none.
   With `card` false only baked art is tried (the PNG decoder is busy with
   another cover), and `*card_pending` says whether the card is worth trying
   later. */
static component_boxart_t *art_load (menu_t *menu, int index, file_image_type_t *which, bool card, bool *card_pending) {
    if (card_pending) {
        *card_pending = false;
    }
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
        if (!game_index_read_header(path_get(path), code, title)) {
            path_free(path);
            return NULL;
        }

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
        baked_lately = true;
        return art;
    }
    /* A game whose front cover is baked had all its art baked with it, so
       no baked back means it has none: show the front again without
       searching the card (about 17 ms each time the box was turned). */
    if (*which != IMAGE_BOXART_FRONT) {
        art = baked_art_load(code, IMAGE_BOXART_FRONT);
        if (art) {
            *which = IMAGE_BOXART_FRONT;
            baked_lately = true;
            return art;
        }
    }
    if (!card) {
        if (card_pending) {
            *card_pending = true;
        }
        return NULL;
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
    box_since = 0;          /* turned over flat, then opens out into a box again */
    box_from_flat = true;
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
        center->art = art_load(menu, center->index, &side, true, NULL);
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

/* ---------- the side covers' top labels (3D Side Covers) ---------- */

static bool side_boxes_on (void) {
    return features_enabled(FEATURE_BOX_3D) && features_enabled(FEATURE_SIDE_BOXES) && features_enabled(FEATURE_SIDE_COVERS);
}

/* Fetch one side cover's top label a frame, once its cover is in. Only
   from baked art: searching the card for each one would be slow. */
static void slot_tops_update (menu_t *menu) {
    if (!side_boxes_on()) {
        return;
    }
    for (int i = 0; i < SLOT_COUNT; i++) {
        slot_t *slot = &slots[i];
        if (!slot->used || slot->top_tried || !art_ready(slot->art)) {
            continue;
        }
        slot->top_tried = true;
        if (!baked_art_owns(slot->art)) {
            continue;
        }
        const cover_list_t *covers = cover_list_current(menu);
        path_t *path = covers->path(menu, slot->index);
        const game_index_entry_t *known = game_index_find(path_get(path), covers->list[slot->index].size);
        path_free(path);
        if (known) {
            char code[5] = "";
            memcpy(code, known->code, 4);
            slot->top = baked_art_load(code, IMAGE_BOXART_TOP);
            return;     /* one a frame */
        }
    }
}

/* ---------- the selected box's edges (3D Box) ---------- */

/* Fetch the next edge picture of the selected box, once its cover is in and
   nothing else is loading. A game whose cover is baked had all its pictures
   baked with it, so the card is only searched for games whose cover came
   from the card. */
static void edges_update (menu_t *menu, uint64_t now) {
    if (!features_enabled(FEATURE_BOX_3D) || flip != FLIP_NONE) {
        return;
    }
    slot_t *center = slot_find(menu, center_index);
    if (!center || !art_ready(center->art) || any_loading()) {
        return;
    }
    bool baked = baked_art_owns(center->art);
    if (!baked && (now - changed_at) < SIDE_SETTLE_TIME_MS) {
        return;     /* the card is slow to search: wait until the selection has really come to rest */
    }
    for (int i = 0; i < EDGE_COUNT; i++) {
        if (edge_tried[i]) {
            continue;
        }
        edge_tried[i] = true;

        const cover_list_t *covers = cover_list_current(menu);
        path_t *path = covers->path(menu, center_index);
        const game_index_entry_t *known = game_index_find(path_get(path), covers->list[center_index].size);
        path_free(path);
        if (!known) {
            return;     /* homebrew, found by title: no edges */
        }
        char code[5] = "";
        memcpy(code, known->code, 4);

        edges[i] = baked_art_load(code, edge_kinds[i]);
        if (!edges[i] && !baked) {
            edges[i] = ui_components_boxart_init(menu->storage_prefix, code, "", edge_kinds[i]);
        }
        return;     /* one a frame */
    }
}

bool carousel_art_draw_box (menu_t *menu, int index, int x0, int y0, int w, int h) {
    slot_t *slot = slot_find(menu, index);
    if (index != center_index || !slot || !art_ready(slot->art) || flip != FLIP_NONE) {
        return false;
    }

    uint64_t now = get_ticks_ms();
    if (box_since == 0) {
        box_since = now;
    }
    float t = (float) (now - box_since) / BOX_EASE_MS;
    /* With 3D Side Covers it slid in as a box already: no opening out. */
    if (!features_enabled(FEATURE_CAROUSEL_ANIMATION) || t > 1.0f || (side_boxes_on() && !box_from_flat)) {
        t = 1.0f;
    }
    float amount = 1.0f - (1.0f - t) * (1.0f - t);     /* eases out */

    box3d_art_t art = {
        .front = slot->art->image,
        .side = theme_get()->panel,
        .bright = art_bright,
    };
    art.top = art_ready(edges[0]) ? edges[0]->image : (art_ready(slot->top) ? slot->top->image : NULL);
    art.left = art_ready(edges[1]) ? edges[1]->image : NULL;
    art.right = art_ready(edges[2]) ? edges[2]->image : NULL;
    art.back = (side != IMAGE_BOXART_FRONT);
    color_t tint;
    if (carousel_art_tint_state(menu, index, &tint) == ART_TINT_KNOWN) {
        art.side = tint;
    }
    box3d_pose_t pose = { .yaw = BOX_YAW, .pitch = BOX_PITCH, .amount = amount, .fit_yaw = BOX_YAW, .fit_pitch = BOX_PITCH };
    box_motion_pose(&pose.yaw, &pose.pitch, &pose.lift);
    /* The hover eases in once the box has come to rest, so it doesn't jump. */
    float settle = (float) (now - box_since) / HOVER_EASE_MS;
    settle = (settle > 1.0f) ? 1.0f : settle * (2.0f - settle);
    pose.yaw = BOX_YAW + (pose.yaw - BOX_YAW) * settle;
    pose.pitch = BOX_PITCH + (pose.pitch - BOX_PITCH) * settle;
    pose.lift *= amount * settle;
    box3d_draw(&art, x0, y0, w, h, &pose);
    box_motion_shown();
    return true;
}

bool carousel_art_draw_side_box (menu_t *menu, int index, int x0, int y0, int w, int h, float yaw, int alpha) {
    slot_t *slot = slot_find(menu, index);
    if (!side_boxes_on() || !slot || !art_ready(slot->art) || (index == center_index && flip != FLIP_NONE)) {
        return false;
    }
    box3d_art_t art = {
        .front = slot->art->image,
        .side = theme_get()->panel,
        .bright = art_bright,
        .alpha = alpha,
        .back = (index == center_index && side != IMAGE_BOXART_FRONT),
    };
    if (index == center_index && art_ready(edges[0])) {
        art.top = edges[0]->image;
    } else if (art_ready(slot->top)) {
        art.top = slot->top->image;
    }
    color_t tint;
    if (carousel_art_tint_state(menu, index, &tint) == ART_TINT_KNOWN) {
        art.side = tint;
    }
    /* Sized at the selected box's resting angles, so a box turned further
       comes out narrower, as it should. */
    box3d_pose_t pose = { .yaw = yaw, .pitch = BOX_PITCH, .amount = 1.0f, .fit_yaw = BOX_YAW, .fit_pitch = BOX_PITCH };
    box3d_draw(&art, x0, y0, w, h, &pose);
    return true;
}

/* ---------- the cache ---------- */

void carousel_art_reset (void) {
    edges_clear();
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
        edges_clear();
        box_motion_reset();
        box_from_flat = false;
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
        int index = cover_list_neighbor(menu, order[i]);
        if (index >= 0) {
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
    int settle = baked_lately ? BAKED_SETTLE_TIME_MS : SETTLE_TIME_MS;
    int side_settle = baked_lately ? BAKED_SIDE_SETTLE_TIME_MS : SIDE_SETTLE_TIME_MS;
    /* The PNG decoder handles one picture at a time, but baked art does not
       need it: while a picture from the card is still decoding, covers with
       baked art load all the same, and the others take their turn after. */
    bool decoder_free = !any_loading();
    if (flip == FLIP_NONE && (now - changed_at) >= settle) {
        for (int j = 0; j < wanted_count; j++) {
            int index = wanted[j];
            if (index != selected && (now - changed_at) < side_settle) {
                break; /* the side covers wait until the selection has really come to rest */
            }
            slot_t *slot = slot_find(menu, index);
            if (slot) {
                if (!slot->card_pending || !decoder_free) {
                    continue; /* already loaded, known to have no art, or still waiting for the decoder */
                }
            } else {
                slot = slot_free();
                if (!slot) {
                    break;
                }
                entry_t *e = &cover_list_current(menu)->list[index];
                slot->used = true;
                slot->index = index;
                slot->entry = e;
                slot->hash = name_hash(e->name);
                slot->art = NULL;
            }
            slot->card_pending = false;
            if (slot->entry->type == ENTRY_TYPE_ROM && (index == selected || memory_for_side_art())) {
                file_image_type_t which = IMAGE_BOXART_FRONT;
                uint64_t began = get_ticks_ms();
                slot->art = art_load(menu, index, &which, decoder_free, &slot->card_pending);
                debugf("cover: starting one took %d ms\n", (int) (get_ticks_ms() - began));
            }
            if (slot->art) {
                break;
            }
        }
    }

    flip_update(menu);
    edges_update(menu, now);
    slot_tops_update(menu);

    /* Also saved once the selection has rested a while, in case the console
       is switched off without leaving this screen. */
    if ((now - changed_at) >= INDEX_SAVE_REST_MS && !game_learn_busy()    // the reader's games are saved with the rest, once it has finished
        && game_index_changed() && safe_file_idle_turn()) {                // one idle save at a time
        game_index_flush();
    }
}

bool carousel_art_aspect (menu_t *menu, int index, float *aspect) {
    slot_t *slot = slot_find(menu, index);
    if (!slot || !art_ready(slot->art) || slot->art->image->height <= 0) {
        return false;
    }
    *aspect = (float) slot->art->image->width / slot->art->image->height;
    return true;
}

bool carousel_art_tint (menu_t *menu, int index, color_t *out) {
    return carousel_art_tint_state(menu, index, out) == ART_TINT_KNOWN;
}

art_tint_state_t carousel_art_tint_state (menu_t *menu, int index, color_t *out) {
    slot_t *slot = slot_find(menu, index);
    if (!slot || slot->card_pending || (slot->art && slot->art->loading)) {
        return ART_TINT_PENDING;    /* its art has not arrived yet */
    }
    if (!slot->art || !slot->art->image) {
        return ART_TINT_NONE;       /* it has no art */
    }
    if (slot->tint_state == 0) {
        slot->tint_state = art_tint_from(slot->art->image, &slot->tint) ? 2 : 1;
    }
    if (slot->tint_state != 2) {
        return ART_TINT_NONE;
    }
    *out = slot->tint;
    return ART_TINT_KNOWN;
}

void carousel_art_brightness (int bright) {
    art_bright = (bright < 0) ? 0 : ((bright > 255) ? 255 : bright);
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
        if (alpha >= 255 && art_bright >= 255) {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        } else if (alpha >= 255) {
            /* Darkened as it is drawn (the fade-in): costs the chip nothing extra. */
            rdpq_set_prim_color(RGBA32(art_bright, art_bright, art_bright, 0xFF));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        } else {
            /* See-through, like the placeholder covers at the sides. */
            rdpq_set_prim_color(RGBA32(art_bright, art_bright, art_bright, alpha));
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

/* One corner of an angled cover: screen position, then the art's position
   and how near the edge is (1 = full size, smaller = further away), which
   the chip uses to keep the picture's perspective right. */
static void angled_vertex (float *v, float x, float y, float s, float t, float near) {
    v[0] = x;
    v[1] = y;
    v[2] = s;
    v[3] = t;
    v[4] = near;
}

bool carousel_art_draw_angled (menu_t *menu, int index, int x0, int y0, int w, int h, float left, float right, int alpha) {
    slot_t *slot = slot_find(menu, index);
    if (!slot || !art_ready(slot->art)) {
        return false;
    }
    surface_t *image = slot->art->image;
    tex_format_t format = surface_get_format(image);
    if (format != FMT_RGBA16 && format != FMT_RGBA32) {
        return carousel_art_draw(menu, index, x0, y0, w, h, alpha, false);
    }
    if (w < 2) {
        return true;
    }

    /* The art is far larger than the chip's 4 KB texture memory, so it is
       drawn in bands of rows, each one loaded and drawn in turn. A band
       loads one row more than it draws, so smoothing has its neighbor. */
    int iw = image->width;
    int ih = image->height;
    int line_bytes = ((iw * 2) + 7) & ~7;           /* RGBA32 is split in two halves of 16 bits */
    int capacity = (format == FMT_RGBA32) ? 2048 : 4096;
    int band = (capacity / line_bytes) - 1;
    if (band < 1) {
        return carousel_art_draw(menu, index, x0, y0, w, h, alpha, false);
    }

    float cy = y0 + h / 2.0f;
    float lh = h * left;
    float rh = h * right;
    float lx = x0;
    float rx = x0 + w;

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_persp(true);
        if (alpha >= 255 && art_bright >= 255) {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        } else if (alpha >= 255) {
            rdpq_set_prim_color(RGBA32(art_bright, art_bright, art_bright, 0xFF));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        } else {
            rdpq_set_prim_color(RGBA32(art_bright, art_bright, art_bright, alpha));
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        for (int t0 = 0; t0 < ih; t0 += band) {
            int t1 = (t0 + band < ih) ? (t0 + band) : ih;
            rdpq_tex_upload_sub(TILE0, image, NULL, 0, t0, iw, (t1 + 1 < ih) ? (t1 + 1) : ih);
            float a = (float) t0 / ih;
            float b = (float) t1 / ih;
            float tl[5], tr[5], br[5], bl[5];
            angled_vertex(tl, lx, cy + lh * (a - 0.5f), 0, t0, left);
            angled_vertex(tr, rx, cy + rh * (a - 0.5f), iw, t0, right);
            angled_vertex(br, rx, cy + rh * (b - 0.5f), iw, t1, right);
            angled_vertex(bl, lx, cy + lh * (b - 0.5f), 0, t1, left);
            rdpq_triangle(&TRIFMT_TEX, tl, tr, br);
            rdpq_triangle(&TRIFMT_TEX, tl, br, bl);
        }
    rdpq_mode_pop();
    return true;
}
