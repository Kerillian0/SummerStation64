#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "avatars.h"
#include "fonts.h"
#include "games_ui.h"
#include "memory_note.h"
#include "menu_options.h"
#include "png_decoder.h"
#include "profiles.h"
#include "title_font.h"
#include "utils/fs.h"

#define BAKED_DIR       "rom:/avatars"
#define BAKED_LIST      BAKED_DIR "/list.txt"
#define CARD_DIR        "sd:/menu/avatars"
#define BAKED_MAX       (48)
#define CARD_MAX        (32)
#define LABEL_LENGTH    (32)
#define SIDE            (64)    /* every picture is this square */
#define SLOTS           (20)    /* a page of the choosing grid and every profile's */
#define MIN_FREE        (96 * 1024)     /* don't load one when memory is this short */
#define CARD_WAIT_MS    (150)   /* a card picture waits for the selection to rest a moment */

typedef struct {
    char stem[8];               /* "a00": rom:/avatars/a00.sprite */
    char label[LABEL_LENGTH];
} baked_t;

static baked_t baked[BAKED_MAX];
static int baked_count = 0;
static bool baked_read = false;
static char card[CARD_MAX][PROFILE_PICTURE_LENGTH - 3];    /* file names, after "sd:" */
static int card_count = 0;
static char ref_out[PROFILE_PICTURE_LENGTH];

typedef enum {
    SLOT_EMPTY,
    SLOT_WAITING,       /* a card picture: the PNG decoder is free in a moment */
    SLOT_DECODING,
    SLOT_READY,
    SLOT_FAILED,        /* not there, or no memory: the initial is drawn */
} slot_state_t;

typedef struct {
    char ref[PROFILE_PICTURE_LENGTH];
    slot_state_t state;
    sprite_t *sprite;           /* a baked picture */
    surface_t *decoded;         /* a card picture */
    surface_t pixels;           /* either one's pixels */
    int s0, t0, side;           /* the square drawn */
    uint64_t asked_at;
    uint64_t used_at;
} slot_t;

static slot_t slots[SLOTS];
static int decoding = -1;       /* the slot the PNG decoder is reading for, or -1 */

const char *avatars_shape_name (int shape) {
    static const char *names[AVATAR_SHAPE_COUNT] = { "Circle", "Rounded Square" };
    return (shape >= 0 && shape < AVATAR_SHAPE_COUNT) ? names[shape] : "";
}

static void read_baked (void) {
    if (baked_read) {
        return;
    }
    baked_read = true;
    FILE *f = fopen(BAKED_LIST, "r");
    if (!f) {
        return;     /* a build without the owner's pictures */
    }
    char line[64];
    while (baked_count < BAKED_MAX && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *tab = strchr(line, '\t');
        if (!tab || (tab - line) >= (int) sizeof(baked[0].stem)) {
            continue;
        }
        *tab = '\0';
        snprintf(baked[baked_count].stem, sizeof(baked[0].stem), "%s", line);
        snprintf(baked[baked_count].label, LABEL_LENGTH, "%s", tab + 1);
        baked_count++;
    }
    fclose(f);
    debugf("avatars: %d built in\n", baked_count);
}

static int compare_names (const void *a, const void *b) {
    return strcasecmp((const char *) a, (const char *) b);
}

void avatars_rescan (void) {
    read_baked();
    card_count = 0;
    dir_t info;
    int result = dir_findfirst(CARD_DIR, &info);
    while (result == 0 && card_count < CARD_MAX) {
        size_t length = strlen(info.d_name);
        if (info.d_type != DT_DIR && info.d_name[0] != '.' && length > 4 && length < sizeof(card[0])
            && strcasecmp(info.d_name + length - 4, ".png") == 0) {
            snprintf(card[card_count++], sizeof(card[0]), "%s", info.d_name);
        }
        result = dir_findnext(CARD_DIR, &info);
    }
    qsort(card, card_count, sizeof(card[0]), compare_names);
    debugf("avatars: %d on the card\n", card_count);
}

int avatars_count (void) {
    read_baked();
    return 1 + baked_count + card_count;
}

const char *avatars_ref (int choice) {
    read_baked();
    if (choice >= 1 && choice <= baked_count) {
        snprintf(ref_out, sizeof(ref_out), "rom:%s", baked[choice - 1].label);
    } else if (choice > baked_count && choice < avatars_count()) {
        snprintf(ref_out, sizeof(ref_out), "sd:%s", card[choice - 1 - baked_count]);
    } else {
        ref_out[0] = '\0';
    }
    return ref_out;
}

int avatars_find (const char *ref) {
    if (!ref || !ref[0]) {
        return 0;
    }
    for (int i = 1; i < avatars_count(); i++) {
        if (strcmp(avatars_ref(i), ref) == 0) {
            return i;
        }
    }
    return 0;
}

const char *avatars_label (const char *ref) {
    static char label[LABEL_LENGTH];
    if (!ref || !ref[0]) {
        return "None";
    }
    if (strncmp(ref, "rom:", 4) == 0) {
        return ref + 4;
    }
    snprintf(label, sizeof(label), "%s", ref + 3);
    char *dot = strrchr(label, '.');
    if (dot && dot != label) {
        *dot = '\0';
    }
    return label;
}

/* ---------- loading ---------- */

static void slot_free (slot_t *s) {
    if (s->state == SLOT_DECODING && decoding == (int) (s - slots)) {
        png_decoder_abort();
        decoding = -1;
    }
    if (s->sprite) {
        sprite_free(s->sprite);
    }
    if (s->decoded) {
        surface_free(s->decoded);
        free(s->decoded);
    }
    memset(s, 0, sizeof(*s));
}

static bool shape_keeps (int x, int y, int side, int shape) {
    /* Measured from the middle of the pixel, in halves, so it is exact either side. */
    int dx = (2 * x) + 1 - side;
    int dy = (2 * y) + 1 - side;
    if (shape == AVATAR_SHAPE_SQUARE) {
        int r = side / 4;                   /* a corner's radius, in halves: side / 8 pixels */
        int cx = abs(dx) - (side - r);
        int cy = abs(dy) - (side - r);
        return cx <= 0 || cy <= 0 || (cx * cx) + (cy * cy) <= (r * r);
    }
    return (dx * dx) + (dy * dy) <= (side * side);
}

/* Cut the square to the shape: pixels outside it lose their "there" bit. */
static void cut (slot_t *s) {
    int shape = options_get(OPTION_AVATAR_SHAPE);
    s->side = (s->pixels.width < s->pixels.height) ? s->pixels.width : s->pixels.height;
    s->s0 = (s->pixels.width - s->side) / 2;
    s->t0 = (s->pixels.height - s->side) / 2;
    if (surface_get_format(&s->pixels) != FMT_RGBA16) {
        return;
    }
    for (int y = 0; y < s->side; y++) {
        uint16_t *row = (uint16_t *) ((uint8_t *) s->pixels.buffer + ((s->t0 + y) * s->pixels.stride)) + s->s0;
        for (int x = 0; x < s->side; x++) {
            if (!shape_keeps(x, y, s->side, shape)) {
                row[x] &= ~1;
            }
        }
    }
    data_cache_hit_writeback(s->pixels.buffer, s->pixels.stride * s->pixels.height);
}

static bool memory_ok (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if ((heap.total - heap.used) < MIN_FREE) {
        memory_note_cut(MEMORY_CUT_AVATAR);
        return false;
    }
    return true;
}

static void decoded (png_err_t err, surface_t *image, void *data) {
    slot_t *s = &slots[(int) (intptr_t) data];
    decoding = -1;
    if (err != PNG_OK || !image) {
        debugf("avatars: could not read %s (%d)\n", s->ref, err);
        s->state = SLOT_FAILED;
        return;
    }
    s->decoded = image;
    s->pixels = *image;
    cut(s);
    s->state = SLOT_READY;
}

static void start (slot_t *s) {
    if (strncmp(s->ref, "rom:", 4) == 0) {
        s->state = SLOT_FAILED;
        for (int i = 0; i < baked_count; i++) {
            if (strcmp(baked[i].label, s->ref + 4) == 0) {
                char path[64];
                snprintf(path, sizeof(path), BAKED_DIR "/%s.sprite", baked[i].stem);
                if (file_exists(path) && memory_ok()) {     /* sprite_load stops the menu when it fails */
                    s->sprite = sprite_load(path);
                    s->pixels = sprite_get_pixels(s->sprite);
                    cut(s);
                    s->state = SLOT_READY;
                }
                break;
            }
        }
        return;
    }
    if (decoding >= 0 || (get_ticks_ms() - s->asked_at) < CARD_WAIT_MS) {
        return;     /* one card picture at a time */
    }
    char path[96];
    snprintf(path, sizeof(path), CARD_DIR "/%s", s->ref + 3);
    if (!memory_ok()) {
        s->state = SLOT_FAILED;
        return;
    }
    png_err_t err = png_decoder_start(path, SIDE, SIDE, decoded, (void *) (intptr_t) (s - slots));
    if (err == PNG_OK) {
        s->state = SLOT_DECODING;
        decoding = (int) (s - slots);
    } else if (err != PNG_ERR_BUSY) {   /* busy: a cover is being read; try again next time */
        debugf("avatars: could not open %s (%d)\n", path, err);
        s->state = SLOT_FAILED;
    }
}

/* The slot for a picture, taking the one used longest ago when all are in use. */
static slot_t *slot_for (const char *ref) {
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i].state != SLOT_EMPTY && strcmp(slots[i].ref, ref) == 0) {
            return &slots[i];
        }
    }
    slot_t *oldest = &slots[0];
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i].state == SLOT_EMPTY) {
            oldest = &slots[i];
            break;
        }
        if (slots[i].used_at < oldest->used_at) {
            oldest = &slots[i];
        }
    }
    slot_free(oldest);
    snprintf(oldest->ref, sizeof(oldest->ref), "%s", ref);
    oldest->state = SLOT_WAITING;
    oldest->asked_at = get_ticks_ms();
    return oldest;
}

/* ---------- drawing ---------- */

static void shape_fill (int x, int y, int size, color_t color) {
    static sprite_t *disc = NULL;
    if (options_get(OPTION_AVATAR_SHAPE) == AVATAR_SHAPE_SQUARE) {
        games_ui_round_fill(x, y, x + size, y + size, color);
        return;
    }
    if (!disc) {
        disc = sprite_load("rom:/avatar_disc.sprite");
    }
    rdpq_mode_push();
        /* The disc's shades say how much of each pixel is covered, which
           gives it a smooth edge; the color comes from here. */
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_set_prim_color(color);
        float scale = (float) size / SIDE;
        rdpq_sprite_blit(disc, x, y, &(rdpq_blitparms_t) { .scale_x = scale, .scale_y = scale, .filtering = true });
    rdpq_mode_pop();
}

static void initial_draw (int x, int y, int size, const char *name) {
    char letter[8] = "?";
    if (name && name[0]) {
        int n = 1;
        while (n < 4 && (name[n] & 0xC0) == 0x80) {
            n++;    /* the whole of a letter written in several bytes */
        }
        snprintf(letter, sizeof(letter), "%.*s", n, name);
        if (n == 1) {
            letter[0] = (char) toupper((unsigned char) letter[0]);
        }
    }
    uint8_t font = (size >= 36) ? title_font_pick(letter, size) : FNT_DEFAULT;
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = size,
        .height = size,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .style_id = STL_WHITE,
    }, font, x, y, "%s", letter);
}

void avatars_draw (const char *ref, int x, int y, int size, color_t color, const char *name) {
    read_baked();
    shape_fill(x, y, size, color);

    slot_t *s = NULL;
    if (ref && ref[0]) {
        s = slot_for(ref);
        s->used_at = get_ticks_ms();
        if (s->state == SLOT_WAITING) {
            start(s);
        }
    }
    if (!s || s->state != SLOT_READY) {
        initial_draw(x, y, size, name);
        return;
    }

    int ring = (size >= 40) ? 3 : 2;
    int inner = size - (2 * ring);
    float scale = (float) inner / s->side;
    rdpq_mode_push();
        /* Bilinear smoothing, and the cut-away part left out by its
           "there" bit: an edge that follows the shape between pixels. */
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_mode_alphacompare(128);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(&s->pixels, x + ring, y + ring, &(rdpq_blitparms_t) {
            .s0 = s->s0, .t0 = s->t0, .width = s->side, .height = s->side,
            .scale_x = scale, .scale_y = scale, .filtering = true,
        });
    rdpq_mode_pop();
}

void avatars_forget_unused (void) {
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i].state == SLOT_EMPTY) {
            continue;
        }
        bool used = false;
        for (int p = 0; p < profiles_count(); p++) {
            used |= (strcmp(profiles_picture(p), slots[i].ref) == 0);
        }
        if (!used) {
            slot_free(&slots[i]);
        }
    }
}

void avatars_shape_changed (void) {
    for (int i = 0; i < SLOTS; i++) {
        slot_free(&slots[i]);
    }
}
