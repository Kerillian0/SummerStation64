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
   Traced from the pack's cartridge pictures (all alike), clockwise. */
static const float cart_outline[][2] = {
    { 0.0f, 1.0f }, { 0.0f, 0.16f },
    { 0.083f, 0.094f }, { 0.166f, 0.066f }, { 0.248f, 0.038f }, { 0.331f, 0.019f }, { 0.414f, 0.009f },
    { 0.497f, 0.0f },
    { 0.586f, 0.009f }, { 0.669f, 0.019f }, { 0.752f, 0.038f }, { 0.834f, 0.066f }, { 0.917f, 0.094f },
    { 1.0f, 0.16f }, { 1.0f, 1.0f },
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
/* A color the front picture is multiplied by (a cartridge's back, tinted to its plastic). */
static color_t draw_tint = { 255, 255, 255, 255 };

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
    bool tinted = (draw_tint.r & draw_tint.g & draw_tint.b) != 255;
    if (k >= 255 && draw_alpha >= 255 && !tinted) {
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    } else {
        rdpq_set_prim_color(RGBA32(draw_tint.r * k / 255, draw_tint.g * k / 255, draw_tint.b * k / 255, draw_alpha));
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
   per stretch of the outline, each shaded by which way it faces (lit from
   above). Drawn furthest first, so nearer walls cover further ones. */
static void cartridge_walls (float bw, float bh, float bd, float yaw, float pitch, color_t color, int bright) {
    point_t saved[8];       /* the walls borrow the corner list; the front is drawn from it afterwards */
    memcpy(saved, corners, sizeof(saved));
    point_t front[CART_POINTS], back[CART_POINTS];
    for (int i = 0; i < CART_POINTS; i++) {
        float x = (cart_outline[i][0] - 0.5f) * bw;
        float y = (cart_outline[i][1] - 0.5f) * bh;
        front[i] = turn_point(x, y, -bd / 2, yaw, pitch);
        back[i] = turn_point(x, y, bd / 2, yaw, pitch);
    }
    int order[CART_POINTS];
    float depth[CART_POINTS];
    int count = 0;
    for (int i = 0; i < CART_POINTS; i++) {
        int j = (i + 1) % CART_POINTS;
        /* Seen from outside: the same corner order as the box's faces. */
        corners[0] = front[i]; corners[1] = back[i]; corners[2] = back[j]; corners[3] = front[j];
        static const int wall[4] = { 0, 1, 2, 3 };
        if (!facing(wall)) {
            continue;
        }
        float z = front[i].z + front[j].z + back[i].z + back[j].z;
        int k = count++;
        while (k > 0 && depth[k - 1] < z) {     /* furthest first */
            order[k] = order[k - 1];
            depth[k] = depth[k - 1];
            k--;
        }
        order[k] = i;
        depth[k] = z;
    }
    for (int n = 0; n < count; n++) {
        int i = order[n];
        int j = (i + 1) % CART_POINTS;
        /* Which way the wall faces, across the picture: up is lit, down is dark. */
        float dx = cart_outline[j][0] - cart_outline[i][0];
        float dy = cart_outline[j][1] - cart_outline[i][1];
        float length = sqrtf(dx * dx + dy * dy);
        float nx = dy / length, ny = -dx / length;
        float light = -0.45f * nx - 0.89f * ny;
        float shade = 0.5f + 0.4f * ((light > 0.0f) ? light : 0.0f);
        corners[0] = front[i]; corners[1] = back[i]; corners[2] = back[j]; corners[3] = front[j];
        static const int wall[4] = { 0, 1, 2, 3 };
        face_plain(wall, shaded(color, shade, bright));
    }
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

/* Draw a face with its picture, or in plain color when it has none. */
static void face (const int *corners_of, surface_t *image, color_t color, float shade, int bright) {
    if (facing(corners_of) && !(image && face_picture(corners_of, image, shade, bright))) {
        face_plain(corners_of, shaded(color, shade, bright));
    }
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
    float bd = art->cartridge ? bw * CART_DEPTH
        : (art->top && art->top->width > 0) ? bw * art->top->height / art->top->width
        : longest * DEPTH_GUESS;
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
            cartridge_walls(bw, bh, bd, pose->yaw * amount, pose->pitch * amount, art->side, art->bright);
        } else if (amount > 0.0f) {
            /* The bottom carries the same label as the top. */
            face(faces[FACE_TOP], art->top, art->side, TOP_SHADE, art->bright);
            face(faces[FACE_BOTTOM], art->top, art->side, BOTTOM_SHADE, art->bright);
            face(faces[FACE_LEFT], art->back ? art->right : art->left, art->side, SIDE_SHADE, art->bright);
            face(faces[FACE_RIGHT], art->back ? art->left : art->right, art->side, SIDE_SHADE, art->bright);
        }
        if (art->front && facing(faces[FACE_FRONT])) {
            draw_cutout = art->cartridge;
            if (art->front_tint.a) {
                draw_tint = art->front_tint;
            }
            face_picture(faces[FACE_FRONT], art->front, 1.0f, art->bright);
            draw_cutout = false;
            draw_tint = (color_t) { 255, 255, 255, 255 };
        }
    rdpq_mode_pop();
}
