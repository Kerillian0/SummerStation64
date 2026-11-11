#include <math.h>
#include <stdio.h>
#include <libdragon.h>

#include "baked_art.h"
#include "cart_model.h"

#define MIN_FREE            (160 * 1024)    /* sprite_load stops the menu when memory runs out */
#define BACK_PATH           "rom:/cart_back.sprite"
#define BOTTOM_PATH         "rom:/cart_bottom.sprite"
/* The shared back's grey plastic (make_icons.py draws it), and its label, which keeps its own colours. */
#define BACK_PLASTIC        (176)
#define LABEL_X0            (0.14f)
#define LABEL_X1            (0.86f)
#define LABEL_Y0            (0.28f)
#define LABEL_Y1            (0.84f)

static component_boxart_t *front_art = NULL;
static sprite_t *back_sprite = NULL;    /* also the front of a cartridge without a picture */
static surface_t back_surface;
static sprite_t *pcb = NULL;
static surface_t pcb_surface;
static color_t body;

static bool memory_free_for (int bytes) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    return (heap.total - heap.used) >= bytes;
}

static sprite_t *sprite_from_rom (const char *path, surface_t *surface) {
    if (!memory_free_for(MIN_FREE)) {
        return NULL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fclose(f);
    sprite_t *sprite = sprite_load(path);
    if (sprite) {
        *surface = sprite_get_pixels(sprite);
    }
    return sprite;
}

static surface_t *front (void) {
    if (front_art && front_art->image) {
        return front_art->image;
    }
    return back_sprite ? &back_surface : NULL;
}

/* The plastic's colour, from the picture's left grip (as the cover rows do). */
static color_t plastic_of (surface_t *image) {
    color_t grey = RGBA32(0x80, 0x80, 0x84, 0xFF);
    if (!image || surface_get_format(image) != FMT_RGBA16) {
        return grey;
    }
    int w = image->width, h = image->height;
    uint16_t *px = image->buffer;
    int stride = image->stride / 2;
    int r = 0, g = 0, b = 0, n = 0;
    for (int y = h * 35 / 100; y < h * 65 / 100; y += 2) {
        for (int x = w * 2 / 100; x < w * 10 / 100; x++) {
            uint16_t p = px[y * stride + x];
            if (p & 1) {
                r += (p >> 11) & 31; g += (p >> 6) & 31; b += (p >> 1) & 31; n++;
            }
        }
    }
    return n ? RGBA32(r * 255 / (31 * n), g * 255 / (31 * n), b * 255 / (31 * n), 0xFF) : grey;
}

/* The shared back's grey plastic in this cartridge's colour; its label keeps its own. */
static void back_tint (color_t colour) {
    surface_t *image = &back_surface;
    if (surface_get_format(image) != FMT_RGBA16) {
        return;
    }
    int tr = colour.r * 255 / BACK_PLASTIC, tg = colour.g * 255 / BACK_PLASTIC, tb = colour.b * 255 / BACK_PLASTIC;
    tr = (tr > 255) ? 255 : tr; tg = (tg > 255) ? 255 : tg; tb = (tb > 255) ? 255 : tb;
    int w = image->width, h = image->height;
    int lx0 = (int) (w * LABEL_X0), lx1 = (int) (w * LABEL_X1);
    int ly0 = (int) (h * LABEL_Y0), ly1 = (int) (h * LABEL_Y1);
    uint16_t *px = image->buffer;
    int stride = image->stride / 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t p = px[y * stride + x];
            if (!(p & 1) || (x >= lx0 && x < lx1 && y >= ly0 && y < ly1)) {
                continue;
            }
            int r = (((p >> 11) & 31) * tr) / 255;
            int g = (((p >> 6) & 31) * tg) / 255;
            int b = (((p >> 1) & 31) * tb) / 255;
            px[y * stride + x] = (r << 11) | (g << 6) | (b << 1) | 1;
        }
    }
    data_cache_hit_writeback(image->buffer, image->stride * h);
}

void cart_model_free (void) {
    if (!front_art && !back_sprite && !pcb) {
        return;
    }
    rspq_wait();    /* nothing may still be drawing from them */
    if (front_art) {
        baked_art_free(front_art);
        front_art = NULL;
    }
    if (back_sprite) {
        sprite_free(back_sprite);
        back_sprite = NULL;
    }
    if (pcb) {
        sprite_free(pcb);
        pcb = NULL;
    }
}

void cart_model_load (const char *code) {
    cart_model_free();
    if (code && baked_art_has(code, IMAGE_GAMEPAK_FRONT)) {
        front_art = baked_art_load(code, IMAGE_GAMEPAK_FRONT);
    }
    back_sprite = sprite_from_rom(BACK_PATH, &back_surface);
    pcb = sprite_from_rom(BOTTOM_PATH, &pcb_surface);
    body = plastic_of(front());
    if (back_sprite && cart_model_own()) {
        back_tint(body);    /* a plain grey cartridge keeps its grey */
    }
}

bool cart_model_ready (void) {
    return front() != NULL;
}

bool cart_model_own (void) {
    return front_art && front_art->image;
}

float cart_model_shape (void) {
    surface_t *picture = front();
    return (picture && picture->width > 0) ? (float) picture->height / picture->width : 0.65f;
}

void cart_model_draw (const box3d_pose_t *pose, int x0, int y0, int w, int h, float turn, int light, int alpha) {
    surface_t *picture = front();
    if (!picture) {
        return;
    }
    turn = fmodf(turn, 360.0f);
    turn = (turn > 180.0f) ? turn - 360.0f : (turn < -180.0f) ? turn + 360.0f : turn;
    if (turn > 90.0f || turn < -90.0f) {
        picture = back_sprite ? &back_surface : picture;    /* its back */
        turn += (turn > 0.0f) ? -180.0f : 180.0f;
    }
    box3d_art_t art = {
        .front = picture,
        .side = body,
        .bright = light,
        .alpha = alpha,
        .cartridge = true,
        .bottom = pcb ? &pcb_surface : NULL,
    };
    box3d_pose_t p = *pose;
    p.yaw = turn;
    p.open = 0.0f;
    p.parts = 0;
    if (!p.world) {
        p.fit_yaw = 0.0f;       /* sized facing front, so it keeps one size as it turns */
        p.fit_pitch = p.pitch;
    }
    box3d_draw(&art, x0, y0, w, h, &p);
}
