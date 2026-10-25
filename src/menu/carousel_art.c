#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory_note.h"
#include "carousel_art.h"
#include "retro.h"
#include "art_tint.h"
#include "game_learn.h"
#include "baked_art.h"
#include "box3d.h"
#include "box_motion.h"
#include "theme.h"
#include "menu_options.h"
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
/* Baked art needs no room to unpack a picture, only the cover itself
   (about 35 KB), so it can go on down to the reserve plus one cover. */
#define BAKED_SIDE_ART_MIN_FREE (288 * 1024)

/* Half of a flip: the time to squash the box flat, and again to open it. */
#define FLIP_HALF_MS        (110)
/* With the 3D Box the box spins round instead: a little slower, so the turn reads. */
#define BOX_FLIP_HALF_MS    (180)

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
    bool cart;                  /* the art is a cartridge (Show Games As: Cartridges), with see-through edges */
    bool cart_ready;            /* its edges are see-through and its plastic color is known */
    color_t body;               /* the cartridge's plastic color, for its 3D edges */
    float outline[BOX3D_OUTLINE_MAX][2];    /* a retro cartridge's outline, traced from its picture (box3d.h) */
    int outline_points;         /* 0: the N64 cartridge's outline */
    bool plain_back;            /* a retro cartridge turned over with no back picture: its front painted plain */
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

const char *carousel_art_style_name (int choice) {
    static const char *names[COVER_STYLE_COUNT] = { "Boxes", "Cartridges" };
    return (choice >= 0 && choice < COVER_STYLE_COUNT) ? names[choice] : "";
}

/* Which picture a cover starts from: the box's front, or the cartridge. */
static file_image_type_t front_kind (void) {
    return (options_get(OPTION_COVER_STYLE) == COVER_STYLE_CARTRIDGES) ? IMAGE_GAMEPAK_FRONT : IMAGE_BOXART_FRONT;
}

/* A cartridge read from the card comes out solid: the unpacker drops the
   picture's see-through parts, which become black. Clear the black that
   reaches the edges (the corners and notches round the cartridge). Then
   take the plastic's color from the cartridge's left grip, for its 3D
   edges. Baked cartridges keep their own see-through edges. */
/* A retro cartridge's outline, from its picture's see-through edges: the
   left and right edges of the solid part at a few heights (closer
   together near the top and bottom, where corners round off), clockwise
   from the bottom left as box3d.h wants it. */
static void cart_trace (slot_t *slot, const uint16_t *px, int stride, int w, int h) {
    static const float heights[] = { 0.0f, 0.02f, 0.05f, 0.10f, 0.20f, 0.35f, 0.50f, 0.65f, 0.80f, 0.90f, 0.95f, 0.98f, 1.0f };
    const int rows = (int) (sizeof(heights) / sizeof(heights[0]));
    int top = 0, bottom = h - 1;
    #define SOLID_ROW(y) ({ bool any = false; for (int x = 0; x < w && !any; x++) any = (px[(y) * stride + x] & 1); any; })
    while (top < bottom && !SOLID_ROW(top)) top++;
    while (bottom > top && !SOLID_ROW(bottom)) bottom--;
    #undef SOLID_ROW
    if (bottom - top < 4) {
        return;
    }
    float left[16][2], right[16][2];
    int n = 0;
    for (int i = 0; i < rows; i++) {
        int y = top + (int) (heights[i] * (bottom - top));
        int x0 = 0, x1 = w - 1;
        while (x0 < w && !(px[y * stride + x0] & 1)) x0++;
        while (x1 > x0 && !(px[y * stride + x1] & 1)) x1--;
        if (x0 >= x1) {
            continue;
        }
        left[n][0] = (float) x0 / w;
        right[n][0] = (float) (x1 + 1) / w;
        left[n][1] = right[n][1] = (float) y / (h - 1);
        n++;
    }
    if (n < 2) {
        return;
    }
    int k = 0;
    for (int i = n - 1; i >= 0; i--) {     /* up the left side */
        slot->outline[k][0] = left[i][0];
        slot->outline[k][1] = left[i][1];
        k++;
    }
    for (int i = 0; i < n; i++) {          /* down the right side; back to the start is the bottom */
        slot->outline[k][0] = right[i][0];
        slot->outline[k][1] = right[i][1];
        k++;
    }
    slot->outline_points = k;
}

static void cart_prepare (slot_t *slot) {
    surface_t *image = slot->art->image;
    slot->cart_ready = true;
    slot->body = RGBA32(0x80, 0x80, 0x84, 0xFF);    /* grey plastic, if the picture says nothing */
    if (surface_get_format(image) != FMT_RGBA16) {
        return;
    }
    int w = image->width, h = image->height;
    uint16_t *px = image->buffer;
    int stride = image->stride / 2;
    #define DARK(p) ((((p) >> 11) & 31) <= 1 && (((p) >> 6) & 31) <= 1 && (((p) >> 1) & 31) <= 1)
    if (!baked_art_owns(slot->art)) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w && DARK(px[y * stride + x]); x++) px[y * stride + x] = 0;
            for (int x = w - 1; x >= 0 && DARK(px[y * stride + x]); x--) px[y * stride + x] = 0;
        }
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h && (px[y * stride + x] == 0 || DARK(px[y * stride + x])); y++) px[y * stride + x] = 0;
            for (int y = h - 1; y >= 0 && (px[y * stride + x] == 0 || DARK(px[y * stride + x])); y--) px[y * stride + x] = 0;
        }
        data_cache_hit_writeback(image->buffer, image->stride * h);
    }
    #undef DARK
    int r = 0, g = 0, b = 0, n = 0;
    for (int y = h * 35 / 100; y < h * 65 / 100; y += 2) {
        for (int x = w * 2 / 100; x < w * 10 / 100; x++) {
            uint16_t p = px[y * stride + x];
            if (p & 1) {
                r += (p >> 11) & 31; g += (p >> 6) & 31; b += (p >> 1) & 31; n++;
            }
        }
    }
    if (n > 0) {
        slot->body = RGBA32(r * 255 / (31 * n), g * 255 / (31 * n), b * 255 / (31 * n), 0xFF);
    }
    slot->outline_points = 0;
    if (slot->entry && slot->entry->type == ENTRY_TYPE_EMULATOR) {
        cart_trace(slot, px, stride, w, h);
        if (slot->plain_back) {
            /* No back picture: the same shape in plain plastic, a little darker than the front. */
            uint16_t plain = ((slot->body.r * 27 / 255) << 11) | ((slot->body.g * 27 / 255) << 6) | ((slot->body.b * 27 / 255) << 1) | 1;  /* 5 bits each, 85% */
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    if (px[y * stride + x] & 1) {
                        px[y * stride + x] = plain;
                    }
                }
            }
            data_cache_hit_writeback(image->buffer, image->stride * h);
        }
    }
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

static bool memory_free_for (int bytes) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    return (heap.total - heap.used) >= bytes;
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

    /* An emulator game: its picture by file name (retro.h), from the card only. */
    if (covers->list[index].type == ENTRY_TYPE_EMULATOR) {
        component_boxart_t *art = NULL;
        if (!card) {
            if (card_pending) {
                *card_pending = true;
            }
        } else {
            if (*which == IMAGE_GAMEPAK_FRONT) {
                art = retro_art_load(menu->storage_prefix, path_get(path), IMAGE_GAMEPAK_FRONT);
                if (!art) {
                    *which = IMAGE_BOXART_FRONT;    /* no cartridge picture: the box */
                }
            } else if (*which != IMAGE_BOXART_BACK) {
                *which = IMAGE_BOXART_FRONT;
            }
            if (!art) {
                art = retro_art_load(menu->storage_prefix, path_get(path), *which);
            }
            if (!art && *which != IMAGE_BOXART_FRONT) {
                *which = IMAGE_BOXART_FRONT;
                art = retro_art_load(menu->storage_prefix, path_get(path), *which);
            }
        }
        path_free(path);
        return art;
    }

    /* A game seen before needs no look at its file: the list knows its code. */
    const game_index_entry_t *known = game_index_find(path_get(path), size);
    if (known && !game_index_is_homebrew(known->code)) {
        memcpy(code, known->code, 4);
    } else {
        /* Homebrew's art is found by its title, which only the file has. */
        if (!game_index_read_header(path_get(path), code, title)) {
            path_free(path);
            return NULL;
        }
        if (!known) {
            game_index_set_code(path_get(path), size, code);
        }
        game_index_homebrew_by_title(code);    /* homebrew without a proper code: look its art up by title too */
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

    if (!baked_art_card_ok(menu->storage_prefix, code)) {
        return NULL;    /* only another region's cover on the card */
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

/* ---------- the back of a cartridge ---------- */

/* Every cartridge's back is the same picture (rom:/cart_back.sprite, drawn
   by assets/images/make_icons.py) in grey plastic. When a cartridge is
   turned over, the plastic round the label is colored once to match that
   cartridge (a gold Zelda gets a gold back); the label keeps its own grey.
   Loaded the first time a cartridge is turned over, loaded again for one
   of another color, let go when the covers are left. */
#define CART_BACK_PATH      "rom:/cart_back.sprite"
#define CART_BACK_PLASTIC   (176)           /* the picture's own plastic grey */
#define CART_BACK_MIN_FREE  (160 * 1024)
/* The label and its rim, across the picture (make_icons.py draws it there). */
#define CART_LABEL_X0       (0.14f)
#define CART_LABEL_X1       (0.86f)
#define CART_LABEL_Y0       (0.28f)
#define CART_LABEL_Y1       (0.84f)
static sprite_t *cart_back_sprite = NULL;
static surface_t cart_back_surface;
static color_t cart_back_color;     /* the plastic color it was given */

/* The color of this cartridge's plastic, against the picture's grey. */
static color_t cart_back_tint (slot_t *slot) {
    int r = slot->body.r * 255 / CART_BACK_PLASTIC;
    int g = slot->body.g * 255 / CART_BACK_PLASTIC;
    int b = slot->body.b * 255 / CART_BACK_PLASTIC;
    return RGBA32(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, 0xFF);
}

static void cart_back_unload (void) {
    if (cart_back_sprite) {
        rspq_wait();    /* nothing may still be drawing from it */
        sprite_free(cart_back_sprite);
        cart_back_sprite = NULL;
    }
}

static bool cart_back_load (slot_t *slot) {
    color_t tint = cart_back_tint(slot);
    if (cart_back_sprite && tint.r == cart_back_color.r && tint.g == cart_back_color.g && tint.b == cart_back_color.b) {
        return true;
    }
    cart_back_unload();     /* colored for another cartridge: start again from the grey */
    if (!memory_free_for(CART_BACK_MIN_FREE)) {
        return false;   /* sprite_load stops the menu if memory runs out */
    }
    FILE *f = fopen(CART_BACK_PATH, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    cart_back_sprite = sprite_load(CART_BACK_PATH);
    cart_back_surface = sprite_get_pixels(cart_back_sprite);
    cart_back_color = tint;

    surface_t *image = &cart_back_surface;
    if (surface_get_format(image) == FMT_RGBA16) {
        int w = image->width, h = image->height;
        int lx0 = (int) (w * CART_LABEL_X0), lx1 = (int) (w * CART_LABEL_X1);
        int ly0 = (int) (h * CART_LABEL_Y0), ly1 = (int) (h * CART_LABEL_Y1);
        uint16_t *px = image->buffer;
        int stride = image->stride / 2;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (x >= lx0 && x < lx1 && y >= ly0 && y < ly1) {
                    continue;   /* the label keeps its colors */
                }
                uint16_t p = px[y * stride + x];
                if (!(p & 1)) {
                    continue;
                }
                int r = (((p >> 11) & 31) * tint.r) / 255;
                int g = (((p >> 6) & 31) * tint.g) / 255;
                int b = (((p >> 1) & 31) * tint.b) / 255;
                px[y * stride + x] = (r << 11) | (g << 6) | (b << 1) | 1;
            }
        }
        data_cache_hit_writeback(image->buffer, image->stride * h);
    }
    return true;
}

/* A cartridge's bottom with its circuit board (rom:/cart_bottom.sprite, 2 KB),
   loaded the first time a 3D cartridge is drawn. */
#define CART_BOTTOM_PATH    "rom:/cart_bottom.sprite"
static sprite_t *cart_bottom_sprite = NULL;
static surface_t cart_bottom_surface;
static bool cart_bottom_tried = false;

static surface_t *cart_bottom (void) {
    if (!cart_bottom_sprite && !cart_bottom_tried) {
        cart_bottom_tried = true;
        FILE *f = fopen(CART_BOTTOM_PATH, "rb");
        if (f && memory_free_for(CART_BACK_MIN_FREE)) {
            cart_bottom_sprite = sprite_load(CART_BOTTOM_PATH);
            cart_bottom_surface = sprite_get_pixels(cart_bottom_sprite);
        }
        if (f) {
            fclose(f);
        }
    }
    return cart_bottom_sprite ? &cart_bottom_surface : NULL;
}

static void cart_back_free (void) {
    cart_back_unload();
    if (cart_bottom_sprite) {
        rspq_wait();
        sprite_free(cart_bottom_sprite);
        cart_bottom_sprite = NULL;
    }
    cart_bottom_tried = false;
}

/* The picture a cover shows right now: a turned-over cartridge shows the shared back. */
static bool shows_cart_back (slot_t *slot) {
    return slot->cart && slot->index == center_index && side != IMAGE_BOXART_FRONT && cart_back_sprite
        && !(slot->entry && slot->entry->type == ENTRY_TYPE_EMULATOR);     /* a retro cartridge has its own back */
}

static surface_t *slot_image (slot_t *slot) {
    return shows_cart_back(slot) ? &cart_back_surface : slot->art->image;
}

/* ---------- flipping the selected box ---------- */

bool carousel_art_flip (void) {
    if (flip != FLIP_NONE || !art_menu) {
        return false;
    }
    slot_t *center = slot_find(art_menu, center_index);
    bool retro_cart = center && center->cart && center->entry && center->entry->type == ENTRY_TYPE_EMULATOR;
    if (!center || !art_ready(center->art) || (center->cart && !retro_cart && !cart_back_load(center))) {
        return false;
    }
    flip = FLIP_CLOSING;
    flip_at = get_ticks_ms();
    if (!features_enabled(FEATURE_BOX_3D)) {
        box_since = 0;      /* turned over flat, then opens out into a box again */
        box_from_flat = true;
    }
    return true;
}

/* Moves the flip along. With the slide animation switched off the flip is instant too. */
static void flip_update (menu_t *menu) {
    if (flip == FLIP_NONE) {
        return;
    }

    uint64_t half = !features_enabled(FEATURE_CAROUSEL_ANIMATION) ? 0 : (features_enabled(FEATURE_BOX_3D) ? BOX_FLIP_HALF_MS : FLIP_HALF_MS);
    uint64_t now = get_ticks_ms();
    slot_t *center = slot_find(menu, center_index);

    if (!center) {
        flip = FLIP_NONE;
        return;
    }

    if (flip == FLIP_CLOSING && (now - flip_at) >= half && center->cart && center->entry && center->entry->type == ENTRY_TYPE_EMULATOR) {
        /* A retro cartridge: its own back picture (retro.h), else its front painted plain. */
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (&slots[i] != center && slots[i].used && slots[i].art && slots[i].art->loading) {
                slot_clear(&slots[i]);  /* the decoder handles one picture at a time */
            }
        }
        if (center->art) {
            ui_components_boxart_free(center->art);
        }
        side = (side == IMAGE_BOXART_FRONT) ? IMAGE_BOXART_BACK : IMAGE_BOXART_FRONT;
        path_t *path = cover_list_current(menu)->path(menu, center->index);
        center->art = NULL;
        center->plain_back = false;
        if (side != IMAGE_BOXART_FRONT) {
            center->art = retro_art_load(menu->storage_prefix, path_get(path), IMAGE_GAMEPAK_BACK);
            center->plain_back = (center->art == NULL);
        }
        if (!center->art) {
            center->art = retro_art_load(menu->storage_prefix, path_get(path), IMAGE_GAMEPAK_FRONT);
        }
        path_free(path);
        center->cart_ready = false;     /* its edges cleared and outline traced again */
        flip = FLIP_LOADING;
    }

    if (flip == FLIP_CLOSING && (now - flip_at) >= half && center->cart) {
        /* A cartridge keeps its front: the shared back is shown in its place. */
        side = (side == IMAGE_BOXART_FRONT) ? IMAGE_BOXART_BACK : IMAGE_BOXART_FRONT;
        flip = FLIP_OPENING;
        flip_at = now;
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
    float t = (float) (get_ticks_ms() - flip_at) / (features_enabled(FEATURE_BOX_3D) ? BOX_FLIP_HALF_MS : FLIP_HALF_MS);
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
        if (!slot->used || slot->top_tried || slot->cart || !art_ready(slot->art)) {
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
/* Games whose top or side pictures were looked for on the card and aren't
   there, so coming back to them doesn't search again. Lasts while the
   console is on; the oldest is forgotten first. */
#define EDGES_MISSING_KEPT  (32)
static struct { char code[4]; uint8_t missing; } edges_missing[EDGES_MISSING_KEPT];
static int edges_missing_next = 0;

static bool edge_known_missing (const char *code, int kind) {
    for (int i = 0; i < EDGES_MISSING_KEPT; i++) {
        if (memcmp(edges_missing[i].code, code, 4) == 0) {
            return (edges_missing[i].missing & (1 << kind)) != 0;
        }
    }
    return false;
}

static void edge_note_missing (const char *code, int kind) {
    for (int i = 0; i < EDGES_MISSING_KEPT; i++) {
        if (memcmp(edges_missing[i].code, code, 4) == 0) {
            edges_missing[i].missing |= 1 << kind;
            return;
        }
    }
    memcpy(edges_missing[edges_missing_next].code, code, 4);
    edges_missing[edges_missing_next].missing = 1 << kind;
    edges_missing_next = (edges_missing_next + 1) % EDGES_MISSING_KEPT;
}

static void edges_update (menu_t *menu, uint64_t now) {
    if (!features_enabled(FEATURE_BOX_3D) || flip != FLIP_NONE) {
        return;
    }
    slot_t *center = slot_find(menu, center_index);
    if (!center || !art_ready(center->art) || center->cart || any_loading()) {
        return;     /* a cartridge has no top label or side pictures */
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
        if (edge_kinds[i] == IMAGE_BOXART_TOP && baked && side_boxes_on()) {
            continue;   /* 3D Side Covers fetches every cover's top label already */
        }

        const cover_list_t *covers = cover_list_current(menu);
        path_t *path = covers->path(menu, center_index);
        if (covers->list[center_index].type == ENTRY_TYPE_EMULATOR) {
            /* An emulator game: one spine picture for both sides (drawn on the right too, carousel_art_draw_box()), no top. */
            if (edge_kinds[i] == IMAGE_BOXART_LEFT) {
                edges[i] = retro_art_load(menu->storage_prefix, path_get(path), IMAGE_BOXART_LEFT);
            }
            path_free(path);
            return;     /* one a frame */
        }
        const game_index_entry_t *known = game_index_find(path_get(path), covers->list[center_index].size);
        path_free(path);
        if (!known || game_index_is_homebrew(known->code)) {
            return;     /* homebrew, found by title: no edges */
        }
        char code[5] = "";
        memcpy(code, known->code, 4);

        edges[i] = baked_art_load(code, edge_kinds[i]);
        if (!edges[i] && !baked && !edge_known_missing(code, i) && baked_art_card_ok(menu->storage_prefix, code)) {
            edges[i] = ui_components_boxart_init(menu->storage_prefix, code, "", edge_kinds[i]);
            if (!edges[i]) {
                edge_note_missing(code, i);     /* not on the card: don't search again (about 50 ms each) */
            }
        }
        return;     /* one a frame */
    }
}

/* The selected box's front picture size, kept for the moment in a 3D flip
   when one side has been let go and the other isn't in yet. */
static int box_front_w = 0, box_front_h = 0;

bool carousel_art_draw_box (menu_t *menu, int index, int x0, int y0, int w, int h) {
    slot_t *slot = slot_find(menu, index);
    bool flipping = (flip != FLIP_NONE);
    if (index != center_index || !slot) {
        return false;
    }
    bool ready = art_ready(slot->art);
    if (ready) {
        box_front_w = slot->art->image->width;
        box_front_h = slot->art->image->height;
    } else if (!flipping || box_front_w <= 0) {
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
        .front = ready ? slot_image(slot) : NULL,
        .front_w = box_front_w,
        .front_h = box_front_h,
        .side = theme_get()->panel,
        .bright = art_bright,
    };
    art.top = art_ready(edges[0]) ? edges[0]->image : (art_ready(slot->top) ? slot->top->image : NULL);
    art.left = art_ready(edges[1]) ? edges[1]->image : NULL;
    art.right = art_ready(edges[2]) ? edges[2]->image : NULL;
    if (slot->entry && slot->entry->type == ENTRY_TYPE_EMULATOR) {
        art.right = art.left;   /* an emulator game's one spine picture (retro.h) */
    }
    art.back = (side != IMAGE_BOXART_FRONT);
    if (slot->cart) {
        art.cartridge = true;
        art.top = art.left = art.right = NULL;
        art.side = slot->body;
        art.bottom = cart_bottom();
        art.outline = slot->outline_points ? (const float (*)[2]) slot->outline : NULL;
        art.outline_points = slot->outline_points;
    }
    color_t tint;
    if (slot->cart) {
        /* the plastic's own color, set above */
    } else if (slot->tint_state == 2) {
        art.side = slot->tint;      /* worked out already: keeps its color through a flip */
    } else if (carousel_art_tint_state(menu, index, &tint) == ART_TINT_KNOWN) {
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

    /* Turning over: the box spins on the way it already leans until it is
       edge-on, the other side comes in, and it carries on round into place. */
    if (flipping) {
        float f = (float) (now - flip_at) / BOX_FLIP_HALF_MS;
        f = (f > 1.0f) ? 1.0f : ((f < 0.0f) ? 0.0f : f);
        f = f * f * (3.0f - 2.0f * f);      /* eases in and out */
        float rest = pose.yaw;
        pose.yaw = (flip == FLIP_CLOSING) ? rest + (90.0f - rest) * f
                 : (flip == FLIP_LOADING) ? 90.0f
                 : -90.0f + (rest + 90.0f) * f;
        pose.fit_yaw = BOX_YAW;
        if (!features_enabled(FEATURE_CAROUSEL_ANIMATION)) {
            pose.yaw = rest;
        }
        pose.amount = 1.0f;     /* the amount scales the angles: keep the spin whole */
    }
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
    if (slot->cart) {
        art.cartridge = true;
        art.top = NULL;
        art.side = slot->body;
        art.bottom = cart_bottom();
        art.outline = slot->outline_points ? (const float (*)[2]) slot->outline : NULL;
        art.outline_points = slot->outline_points;
    } else if (carousel_art_tint_state(menu, index, &tint) == ART_TINT_KNOWN) {
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
    cart_back_free();
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
            bool side_cover = (index != selected);
            slot_t *slot = slot_find(menu, index);
            if (slot) {
                if (!slot->card_pending || !decoder_free) {
                    continue; /* already loaded, known to have no art, or still waiting for the decoder */
                }
                if (side_cover && !memory_free_for(SIDE_ART_MIN_FREE)) {
                    continue; /* its picture on the card waits until there is room to unpack it */
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
            if (slot->entry->type == ENTRY_TYPE_ROM || slot->entry->type == ENTRY_TYPE_EMULATOR) {    /* emulator games: retro.h */
                if (side_cover && !memory_free_for(BAKED_SIDE_ART_MIN_FREE)) {
                    /* Not even room for a baked cover: leave it untried, so it
                       is tried again once memory has been freed. */
                    memory_note_cut(MEMORY_CUT_SIDE_ART);
                    slot->used = false;
                    continue;
                }
                /* A picture from the card needs room to unpack as well. */
                bool card = decoder_free && (!side_cover || memory_free_for(SIDE_ART_MIN_FREE));
                file_image_type_t which = front_kind();
                uint64_t began = get_ticks_ms();
                slot->art = art_load(menu, index, &which, card, &slot->card_pending);
                slot->cart = (which == IMAGE_GAMEPAK_FRONT);
                debugf("cover: starting one took %d ms\n", (int) (get_ticks_ms() - began));
            }
            if (slot->art) {
                break;
            }
        }
    }

    for (int i = 0; i < SLOT_COUNT; i++) {
        if (slots[i].used && slots[i].cart && !slots[i].cart_ready && art_ready(slots[i].art)) {
            cart_prepare(&slots[i]);
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

    surface_t *image = slot_image(slot);

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
        if (slot->cart) {
            rdpq_mode_alphacompare(128);    /* a cartridge's see-through edges */
        }
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
        if (slot->cart) {
            rdpq_mode_alphacompare(128);
        }
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

bool carousel_art_cutout (menu_t *menu, int index) {
    slot_t *slot = slot_find(menu, index);
    return slot && slot->cart && art_ready(slot->art);
}
