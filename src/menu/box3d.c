/**
 * @file box3d.c
 * @brief Draws a game's cover as a box with depth (3D Box).
 */

#include <math.h>
#include <string.h>

#include "box3d.h"

/* How deep a box is against its longest side when there is no top strip
   to tell (a US box's top strip is 158 x 22). */
#define DEPTH_GUESS         (0.14f)
/* However its pictures are shaped, a box's depth stays within real boxes'
   proportions against its longest side (an N64 box is about 0.19): a tall
   picture used as the top label once made a box three times too deep. */
#define DEPTH_MIN           (0.06f)
#define DEPTH_MAX           (0.25f)
/* A cartridge's thickness against its width (about 18 mm to 116 mm). */
#define CART_DEPTH          (0.16f)
/* The camera's distance, against the box's longest side: nearer gives
   stronger perspective. */
#define CAMERA_DISTANCE     (3.5f)
/* How much darker the top and the sides are than the front. */
#define TOP_SHADE           (0.85f)
#define SIDE_SHADE          (0.62f)
#define BOTTOM_SHADE        (0.45f)

typedef struct { float x, y, z; } point_t;

/* The box's corners: bit 0 right, bit 1 bottom, bit 2 back. */
static point_t corners[8];

/* Each face's corners as seen from outside it: top left, top right,
   bottom right, bottom left. */
enum { FACE_FRONT, FACE_TOP, FACE_BOTTOM, FACE_LEFT, FACE_RIGHT, FACE_COUNT };
static const int faces[FACE_COUNT][4] = {
    [FACE_FRONT] = { 0, 1, 3, 2 },
    [FACE_TOP]   = { 4, 5, 1, 0 },  /* back edge at the top of the strip */
    [FACE_BOTTOM] = { 2, 3, 7, 6 }, /* front edge at the top, so the label reads when the box is tipped up */
    [FACE_LEFT]  = { 4, 0, 2, 6 },  /* back edge at the left */
    [FACE_RIGHT] = { 1, 5, 7, 3 },  /* front edge at the left */
};

/* How see-through the box being drawn is (255: solid). */
static int draw_alpha = 255;
/* Leave out the see-through parts of the picture (a cartridge's edges). */
static bool draw_cutout = false;

/* The outline every N64 cartridge shares, across its picture from (0, 0) at
   the top left to (1, 1): straight sides, a flat bottom, an arched top.
   Clockwise; the last stretch, back to the first point, is the bottom,
   where the circuit board shows. */
static const float cart_outline[][2] = {
/* 15 points, made by assets/images/cart_outline.py */
    { 0.0000f, 1.0000f }, { 0.0000f, 0.1600f }, { 0.0830f, 0.0940f }, { 0.1660f, 0.0660f },
    { 0.2480f, 0.0380f }, { 0.3310f, 0.0190f }, { 0.4140f, 0.0090f }, { 0.4970f, 0.0000f },
    { 0.5860f, 0.0090f }, { 0.6690f, 0.0190f }, { 0.7520f, 0.0380f }, { 0.8340f, 0.0660f },
    { 0.9170f, 0.0940f }, { 1.0000f, 0.1600f }, { 1.0000f, 1.0000f },
};
#define CART_POINTS     ((int) (sizeof(cart_outline) / sizeof(cart_outline[0])))

/* From the box to the screen. */
static float camera;                /* the camera's distance */
static float scale, offset_x, offset_y;
static float nearest;               /* the nearest corner's distance, for the chip's perspective */

static void project (point_t p, float *x, float *y, float *near) {
    float depth = p.z + camera;
    *x = offset_x + (p.x * camera / depth) * scale;
    *y = offset_y + (p.y * camera / depth) * scale;
    *near = nearest / depth;
}

static point_t mix (point_t a, point_t b, float t) {
    return (point_t) { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

/* A face is turned toward the viewer when its corners go round clockwise on the screen. */
static bool facing (const int *face) {
    float x[3], y[3], near;
    for (int i = 0; i < 3; i++) {
        int corner = (i == 2) ? face[3] : face[i];
        project(corners[corner], &x[i], &y[i], &near);
    }
    return ((x[1] - x[0]) * (y[2] - y[0]) - (y[1] - y[0]) * (x[2] - x[0])) > 0.5f;
}

static color_t shaded (color_t c, float shade, int bright) {
    float k = shade * bright / 255.0f;
    return RGBA32((int) (c.r * k), (int) (c.g * k), (int) (c.b * k), 0xFF);
}

static void face_plain (const int *face, color_t color) {
    float v[4][2], near;
    for (int i = 0; i < 4; i++) {
        project(corners[face[i]], &v[i][0], &v[i][1], &near);
    }
    rdpq_set_mode_standard();
    rdpq_set_prim_color(RGBA32(color.r, color.g, color.b, draw_alpha));
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    if (draw_alpha < 255) {
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    }
    rdpq_triangle(&TRIFMT_FILL, v[0], v[1], v[2]);
    rdpq_triangle(&TRIFMT_FILL, v[0], v[2], v[3]);
}

static void vertex (float *v, point_t p, float s, float t) {
    project(p, &v[0], &v[1], &v[4]);
    v[2] = s;
    v[3] = t;
}

/* A face with its picture. The picture is far larger than the chip's 4 KB
   texture memory, so it goes in bands of rows, as for Angled Side Covers.
   @return false if the picture is in a format this can't draw. */
static bool face_picture (const int *face, surface_t *image, float shade, int bright) {
    tex_format_t format = surface_get_format(image);
    if (format != FMT_RGBA16 && format != FMT_RGBA32) {
        return false;
    }
    int iw = image->width;
    int ih = image->height;
    int line_bytes = ((iw * 2) + 7) & ~7;           /* RGBA32 is split in two halves of 16 bits */
    int capacity = (format == FMT_RGBA32) ? 2048 : 4096;
    int band = (capacity / line_bytes) - 1;
    if (band < 1) {
        return false;
    }

    point_t tl = corners[face[0]], tr = corners[face[1]], br = corners[face[2]], bl = corners[face[3]];
    int k = (int) (shade * bright);

    rdpq_set_mode_standard();
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_mode_persp(true);
    if (draw_cutout) {
        rdpq_mode_alphacompare(128);
    }
    if (k > 255) {
        k = 255;
    }
    if (k >= 255 && draw_alpha >= 255) {
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    } else {
        rdpq_set_prim_color(RGBA32(k, k, k, draw_alpha));
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
        if (draw_alpha < 255) {
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
    }
    for (int t0 = 0; t0 < ih; t0 += band) {
        int t1 = (t0 + band < ih) ? (t0 + band) : ih;
        rdpq_tex_upload_sub(TILE0, image, NULL, 0, t0, iw, (t1 + 1 < ih) ? (t1 + 1) : ih);
        float a = (float) t0 / ih;
        float b = (float) t1 / ih;
        float v0[5], v1[5], v2[5], v3[5];
        vertex(v0, mix(tl, bl, a), 0, t0);
        vertex(v1, mix(tr, br, a), iw, t0);
        vertex(v2, mix(tr, br, b), iw, t1);
        vertex(v3, mix(tl, bl, b), 0, t1);
        rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
        rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);
    }
    rdpq_mode_persp(false);
    return true;
}

/* Turn and tip one point, as place() does for the corners. */
static point_t turn_point (float x, float y, float z, float yaw, float pitch) {
    float turn = yaw * (float) M_PI / 180.0f;
    float tip = pitch * (float) M_PI / 180.0f;
    float cy = cosf(turn), sy = sinf(turn), cp = cosf(tip), sp = sinf(tip);
    float x1 = x * cy - z * sy;
    float z1 = x * sy + z * cy;
    return (point_t) { x1, y * cp - z1 * sp, y * sp + z1 * cp };
}

/* A cartridge's edges: its outline pushed back to its thickness, one wall
   per stretch of the outline. Every wall is textured with a patch of
   plastic from the picture on its front, so it is the same material, and
   keeps one shade whichever way the cartridge turns: the top as bright as
   the front, the sides a little darker, the bottom a little more. The
   slot with the circuit board (art->bottom) is laid on the bottom's
   plastic. Drawn furthest first, so nearer walls cover further ones. */
#define CART_SIDE_SHADE     (0.90f)
#define CART_BOTTOM_SHADE   (0.80f)

static void cartridge_walls (const box3d_art_t *art, float bw, float bh, float bd, float yaw, float pitch) {
    point_t saved[8];       /* the walls borrow the corner list; the front is drawn from it afterwards */
    memcpy(saved, corners, sizeof(saved));
    /* Its own outline (a retro cartridge's, traced from its picture), else the N64 one. */
    const float (*outline)[2] = (art->outline && art->outline_points >= 3) ? art->outline : cart_outline;
    int points = (art->outline && art->outline_points >= 3) ? art->outline_points : CART_POINTS;
    if (points > BOX3D_OUTLINE_MAX) {
        points = BOX3D_OUTLINE_MAX;
    }
    point_t front[BOX3D_OUTLINE_MAX], back[BOX3D_OUTLINE_MAX];
    for (int i = 0; i < points; i++) {
        float x = (outline[i][0] - 0.5f) * bw;
        float y = (outline[i][1] - 0.5f) * bh;
        front[i] = turn_point(x, y, -bd / 2, yaw, pitch);
        back[i] = turn_point(x, y, bd / 2, yaw, pitch);
    }

    /* The walls facing the viewer, furthest first. */
    static const int quad[4] = { 0, 1, 2, 3 };
    int order[BOX3D_OUTLINE_MAX];
    float depth[BOX3D_OUTLINE_MAX];
    int count = 0;
    for (int i = 0; i < points; i++) {
        int j = (i + 1) % points;
        corners[0] = front[i]; corners[1] = back[i]; corners[2] = back[j]; corners[3] = front[j];
        if (!facing(quad)) {
            continue;
        }
        float z = front[i].z + front[j].z + back[i].z + back[j].z;
        int k = count++;
        while (k > 0 && depth[k - 1] < z) {
            order[k] = order[k - 1];
            depth[k] = depth[k - 1];
            k--;
        }
        order[k] = i;
        depth[k] = z;
    }

    /* The plastic patch: a little of the picture's left grip. */
    surface_t *plastic = art->front;
    bool textured = plastic && (surface_get_format(plastic) == FMT_RGBA16);
    int s0 = 0, s1 = 0, t0 = 0, t1 = 0;
    if (textured) {
        s0 = plastic->width * 3 / 100; s1 = plastic->width * 8 / 100;
        t0 = plastic->height * 42 / 100; t1 = plastic->height * 58 / 100;
    }
    bool pcb = textured && art->bottom && surface_get_format(art->bottom) == FMT_RGBA16;
    int loaded = 0;     /* which texture the chip holds: 0 none, 1 plastic, 2 circuit board */

    rdpq_set_mode_standard();
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_mode_persp(true);
    rdpq_mode_combiner(textured ? RDPQ_COMBINER_TEX_FLAT : RDPQ_COMBINER_FLAT);
    if (draw_alpha < 255) {
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    }
    for (int n = 0; n < count; n++) {
        int i = order[n];
        int j = (i + 1) % points;
        /* Which way the wall faces, across the picture: up, sideways or down. */
        float dx = outline[j][0] - outline[i][0];
        float dy = outline[j][1] - outline[i][1];
        float shade = (dx > 0.0f) ? 1.0f : (dx < 0.0f) ? CART_BOTTOM_SHADE : CART_SIDE_SHADE;
        (void) dy;
        int k = (int) (shade * art->bright);

        if (!textured) {
            rdpq_set_prim_color(RGBA32(art->side.r * k / 255, art->side.g * k / 255, art->side.b * k / 255, draw_alpha));
        } else {
            if (loaded != 1) {
                rdpq_tex_upload_sub(TILE0, plastic, NULL, s0, t0, s1, t1);
                loaded = 1;
            }
            rdpq_set_prim_color(RGBA32(k, k, k, draw_alpha));
        }
        float v0[5], v1[5], v2[5], v3[5];
        vertex(v0, front[i], s0, t0);
        vertex(v1, back[i], s0, t1);
        vertex(v2, back[j], s1, t1);
        vertex(v3, front[j], s1, t0);
        if (textured) {
            rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
            rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);
        } else {
            float f0[2] = { v0[0], v0[1] }, f1[2] = { v1[0], v1[1] }, f2[2] = { v2[0], v2[1] }, f3[2] = { v3[0], v3[1] };
            rdpq_triangle(&TRIFMT_FILL, f0, f1, f2);
            rdpq_triangle(&TRIFMT_FILL, f0, f2, f3);
        }

        /* The bottom: the slot with the circuit board, in the middle of its plastic. */
        if (pcb && i == points - 1) {
            const float u0 = 0.13f, u1 = 0.87f, w0 = 0.22f, w1 = 0.82f;  /* along the bottom, and front to back */
            point_t near0 = mix(front[i], front[j], u0), near1 = mix(front[i], front[j], u1);
            point_t far0 = mix(back[i], back[j], u0), far1 = mix(back[i], back[j], u1);
            rdpq_tex_upload(TILE0, art->bottom, NULL);
            loaded = 2;
            float bw_ = art->bottom->width, bh_ = art->bottom->height;
            vertex(v0, mix(near0, far0, w0), 0, 0);
            vertex(v1, mix(near0, far0, w1), 0, bh_);
            vertex(v2, mix(near1, far1, w1), bw_, bh_);
            vertex(v3, mix(near1, far1, w0), bw_, 0);
            rdpq_triangle(&TRIFMT_TEX, v0, v1, v2);
            rdpq_triangle(&TRIFMT_TEX, v0, v2, v3);
        }
    }
    rdpq_mode_persp(false);
    memcpy(corners, saved, sizeof(saved));
}

/* Work out the box's corners at the given angles. */
static void place (float bw, float bh, float bd, float yaw, float pitch) {
    float turn = yaw * (float) M_PI / 180.0f;
    float tip = pitch * (float) M_PI / 180.0f;
    float cy = cosf(turn), sy = sinf(turn), cp = cosf(tip), sp = sinf(tip);
    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? bw / 2 : -bw / 2;
        float y = (i & 2) ? bh / 2 : -bh / 2;
        float z = (i & 4) ? bd / 2 : -bd / 2;
        float x1 = x * cy - z * sy;     /* turned: the right edge goes back */
        float z1 = x * sy + z * cy;
        float y2 = y * cp - z1 * sp;    /* tipped: the top comes toward the viewer */
        float z2 = y * sp + z1 * cp;
        corners[i] = (point_t) { x1, y2, z2 };
    }
}

/* A side picture's short and long edges: a spine may come lying down. */
static float side_short (const surface_t *s) { return (s->width < s->height) ? s->width : s->height; }
static float side_long (const surface_t *s) { return (s->width < s->height) ? s->height : s->width; }

/* Draw a face with its picture, or in plain color when it has none. */
static void face (const int *corners_of, surface_t *image, color_t color, float shade, int bright) {
    if (facing(corners_of) && !(image && face_picture(corners_of, image, shade, bright))) {
        face_plain(corners_of, shaded(color, shade, bright));
    }
}

/* A side, top or bottom with its picture turned to fit: a side's picture
   stood upright if it is lying down (wider than tall), a top label laid
   down if it is standing up; turned so its top runs down the face's back
   edge, the way a spine's words read. */
static void strip_face (const int *corners_of, surface_t *image, color_t color, float shade, int bright, bool upright) {
    if (image && (upright ? (image->width > image->height) : (image->height > image->width))) {
        const int turned[4] = { corners_of[1], corners_of[2], corners_of[3], corners_of[0] };
        if (facing(corners_of) && !face_picture(turned, image, shade, bright)) {
            face_plain(corners_of, shaded(color, shade, bright));
        }
        return;
    }
    face(corners_of, image, color, shade, bright);
}

static void side_face (const int *corners_of, surface_t *image, color_t color, float shade, int bright) {
    strip_face(corners_of, image, color, shade, bright, true);
}

/* The top and bottom: a label standing up (taller than wide) is laid down along them. */
static void top_face (const int *corners_of, surface_t *image, color_t color, float shade, int bright) {
    strip_face(corners_of, image, color, shade, bright, false);
}

void box3d_draw (const box3d_art_t *art, int x0, int y0, int w, int h, const box3d_pose_t *pose) {
    if (!art || !pose) {
        return;
    }
    float bw = art->front ? art->front->width : art->front_w;
    float bh = art->front ? art->front->height : art->front_h;
    if (bw <= 0 || bh <= 0 || w < 2 || h < 2) {
        return;
    }
    float amount = pose->amount;
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    draw_alpha = (art->alpha <= 0 || art->alpha > 255) ? 255 : art->alpha;

    /* The box in its own units: the front picture's size, and its depth from
       the top strip's shape, or a guess. */
    float longest = (bw > bh) ? bw : bh;
    surface_t *spine = art->left ? art->left : art->right;
    float bd = art->cartridge ? bw * CART_DEPTH
        : (art->top && art->top->width > 0 && art->top->height > 0) ? bw * side_short(art->top) / side_long(art->top)   /* the top label's shape, lying either way */
        : (spine && spine->width > 0 && spine->height > 0) ? bh * side_short(spine) / side_long(spine)  /* the spine's shape gives the depth */
        : longest * DEPTH_GUESS;
    if (!art->cartridge) {
        bd = (bd < longest * DEPTH_MIN) ? longest * DEPTH_MIN : (bd > longest * DEPTH_MAX) ? longest * DEPTH_MAX : bd;
    }
    bd *= amount;

    /* Its size is worked out at the resting angles, so it keeps that size
       while it sways or is turned by hand, and turns round its middle. */
    camera = longest * CAMERA_DISTANCE;
    scale = 1.0f;
    offset_x = offset_y = 0.0f;
    nearest = 1.0f;
    place(bw, bh, bd, pose->fit_yaw * amount, pose->fit_pitch * amount);
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
    for (int i = 0; i < 8; i++) {
        float x, y, near;
        project(corners[i], &x, &y, &near);
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    float fit_x = w / (max_x - min_x);
    float fit_y = h / (max_y - min_y);
    scale = (fit_x < fit_y) ? fit_x : fit_y;
    offset_x = x0 + w / 2.0f - (min_x + max_x) / 2.0f * scale;
    offset_y = y0 + h / 2.0f - (min_y + max_y) / 2.0f * scale + pose->lift;

    place(bw, bh, bd, pose->yaw * amount, pose->pitch * amount);
    float min_z = 1e9f;
    for (int i = 0; i < 8; i++) {
        if (corners[i].z < min_z) min_z = corners[i].z;
    }
    nearest = min_z + camera;

    rdpq_mode_push();
        if (amount > 0.0f && art->cartridge) {
            cartridge_walls(art, bw, bh, bd, pose->yaw * amount, pose->pitch * amount);
        } else if (amount > 0.0f) {
            /* The bottom carries the same label as the top. */
            top_face(faces[FACE_TOP], art->top, art->side, TOP_SHADE, art->bright);
            top_face(faces[FACE_BOTTOM], art->top, art->side, BOTTOM_SHADE, art->bright);
            side_face(faces[FACE_LEFT], art->back ? art->right : art->left, art->side, SIDE_SHADE, art->bright);
            side_face(faces[FACE_RIGHT], art->back ? art->left : art->right, art->side, SIDE_SHADE, art->bright);
        }
        if (art->front && facing(faces[FACE_FRONT])) {
            draw_cutout = art->cartridge;
            face_picture(faces[FACE_FRONT], art->front, 1.0f, art->bright);
            draw_cutout = false;
        }
    rdpq_mode_pop();
}
