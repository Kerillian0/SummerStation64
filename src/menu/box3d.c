/**
 * @file box3d.c
 * @brief Draws a game's cover as a box with depth (3D Box).
 */

#include <math.h>

#include "box3d.h"

/* How deep a box is against its longest side when there is no top strip
   to tell (a US box's top strip is 158 x 22). */
#define DEPTH_GUESS         (0.14f)
/* The camera's distance, against the box's longest side: nearer gives
   stronger perspective. */
#define CAMERA_DISTANCE     (3.5f)
/* How much darker the top and the sides are than the front. */
#define TOP_SHADE           (0.85f)
#define SIDE_SHADE          (0.62f)

typedef struct { float x, y, z; } point_t;

/* The box's corners: bit 0 right, bit 1 bottom, bit 2 back. */
static point_t corners[8];

/* Each face's corners as seen from outside it: top left, top right,
   bottom right, bottom left. */
enum { FACE_FRONT, FACE_TOP, FACE_LEFT, FACE_RIGHT, FACE_COUNT };
static const int faces[FACE_COUNT][4] = {
    [FACE_FRONT] = { 0, 1, 3, 2 },
    [FACE_TOP]   = { 4, 5, 1, 0 },  /* back edge at the top of the strip */
    [FACE_LEFT]  = { 4, 0, 2, 6 },  /* back edge at the left */
    [FACE_RIGHT] = { 1, 5, 7, 3 },  /* front edge at the left */
};

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
    rdpq_set_prim_color(color);
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
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
    if (k >= 255) {
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    } else {
        rdpq_set_prim_color(RGBA32(k, k, k, 0xFF));
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
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

void box3d_draw (const box3d_art_t *art, int x0, int y0, int w, int h, float yaw, float pitch, float amount) {
    if (!art || !art->front || art->front->width <= 0 || art->front->height <= 0 || w < 2 || h < 2) {
        return;
    }
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;

    /* The box in its own units: the front picture's size, and its depth from
       the top strip's shape, or a guess. */
    float bw = art->front->width;
    float bh = art->front->height;
    float longest = (bw > bh) ? bw : bh;
    float bd = (art->top && art->top->width > 0)
        ? bw * art->top->height / art->top->width
        : longest * DEPTH_GUESS;
    bd *= amount;

    /* Turned and tipped toward the viewer. */
    float turn = yaw * amount * (float) M_PI / 180.0f;
    float tip = pitch * amount * (float) M_PI / 180.0f;
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

    /* Fit the whole box in the space given. */
    camera = longest * CAMERA_DISTANCE;
    scale = 1.0f;
    offset_x = offset_y = 0.0f;
    nearest = 1.0f;
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f, min_z = 1e9f;
    for (int i = 0; i < 8; i++) {
        float x, y, near;
        project(corners[i], &x, &y, &near);
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
        if (corners[i].z < min_z) min_z = corners[i].z;
    }
    float fit_x = w / (max_x - min_x);
    float fit_y = h / (max_y - min_y);
    scale = (fit_x < fit_y) ? fit_x : fit_y;
    offset_x = x0 + w / 2.0f - (min_x + max_x) / 2.0f * scale;
    offset_y = y0 + h / 2.0f - (min_y + max_y) / 2.0f * scale;
    nearest = min_z + camera;

    rdpq_mode_push();
        if (amount > 0.0f) {
            if (facing(faces[FACE_TOP]) && !(art->top && face_picture(faces[FACE_TOP], art->top, TOP_SHADE, art->bright))) {
                face_plain(faces[FACE_TOP], shaded(art->side, TOP_SHADE, art->bright));
            }
            if (facing(faces[FACE_LEFT]) && !(art->left && face_picture(faces[FACE_LEFT], art->left, SIDE_SHADE, art->bright))) {
                face_plain(faces[FACE_LEFT], shaded(art->side, SIDE_SHADE, art->bright));
            }
            if (facing(faces[FACE_RIGHT]) && !(art->right && face_picture(faces[FACE_RIGHT], art->right, SIDE_SHADE, art->bright))) {
                face_plain(faces[FACE_RIGHT], shaded(art->side, SIDE_SHADE, art->bright));
            }
        }
        face_picture(faces[FACE_FRONT], art->front, 1.0f, art->bright);
    rdpq_mode_pop();
}
