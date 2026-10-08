/**
 * @file intro_logo.c
 * @brief The spinning 3D "N" shown in the intro.
 *
 * The same sums were first tried out on a PC, drawing the logo from eight
 * angles, before being written here.
 */

#include <math.h>
#include <stdbool.h>
#include <libdragon.h>

#include "intro_logo.h"

/* The logo is 2 wide (from -1 to 1). */
#define T       (0.60f)                 /* thickness of a pillar */
#define H       (0.96f)                 /* half the height: a little squatter than a cube, as the original is */
#define IN      (1.0f - T)              /* inner edge of a pillar */
/* Where the slanted bar's edges meet the pillars' inner edges. */
#define YA      (-H + (2.0f * H * (2.0f - (2.0f * T)) / (2.0f - T)))
#define YB      (H - (2.0f * H * (2.0f - (2.0f * T)) / (2.0f - T)))

#define TILT    (0.419f)                /* we look down on it a little: 24 degrees */
#define EYE     (6.5f)                  /* how far away we stand; nearer exaggerates the depth */

typedef struct { float x, y, z; } vec_t;

typedef enum {
    COLOR_PILLAR,       /* outer face of a pillar; every other side is COLOR_PILLAR_2 */
    COLOR_PILLAR_2,
    COLOR_BAR,          /* outer face of a slanted bar */
    COLOR_BAR_EDGE,     /* its upper and lower edges */
    COLOR_PILLAR_SIDE,  /* the sides of the pillars that face the gaps */
    COLOR_BAR_INSIDE,   /* a bar seen from the inside */
    COLOR_TOP,
} logo_color_t;

/* Taken from the reference picture. The pillars' outer faces are not a
   flat color: they carry one of two patterns (see below), and these two
   entries only tint them (white = the pattern as it is). */
static const color_t colors[] = {
    [COLOR_PILLAR]      = { 0xFF, 0xFF, 0xFF, 0xFF },
    [COLOR_PILLAR_2]    = { 0xFF, 0xFF, 0xFF, 0xFF },
    [COLOR_BAR]         = { 0xD2, 0x40, 0x41, 0xFF },
    [COLOR_BAR_EDGE]    = { 0x29, 0x8D, 0x8B, 0xFF },
    [COLOR_PILLAR_SIDE] = { 0x7B, 0x77, 0x92, 0xFF },
    [COLOR_BAR_INSIDE]  = { 0x1E, 0x70, 0x70, 0xFF },
    [COLOR_TOP]         = { 0xF3, 0xBE, 0x22, 0xFF },
};

/* The patterns: a marbled "hologram" on two opposite sides and pink-to-blue
   stripes on the other two (rom:/logo_holo.sprite, rom:/logo_stripes.sprite,
   2 KB each, made by assets/images/make_icons.py). Loaded for the intro
   only. */
#define PATTERN_HEIGHT  (64)    /* texels from the top of a pillar to its foot */
static sprite_t *pattern[2] = { NULL, NULL };
static sprite_t *pattern_set = NULL;    /* the one the graphics chip has now */

typedef struct {
    vec_t corner[4];
    vec_t facing;       /* which way the face looks */
    logo_color_t color;
} face_t;

/* One of the four sides, as seen from outside it: x across, y up, z toward
   us. The other three are this one turned by a quarter, a half and three
   quarters. Faces further down the list are drawn over earlier ones. */
#define SLANT   (2.3762f)   /* length of (2H, 2 - T), to make that a unit direction */
static const face_t side[] = {
    /* The sides of the two pillars that face the gap. */
    { { { -IN, -H, IN }, { -IN, -H, 1 }, { -IN, YA, 1 }, { -IN, YA, IN } }, { 1, 0, 0 }, COLOR_PILLAR_SIDE },
    { { { IN, YB, IN }, { IN, YB, 1 }, { IN, H, 1 }, { IN, H, IN } }, { -1, 0, 0 }, COLOR_PILLAR_SIDE },
    /* The bar's upper and lower edges, between the pillars. */
    { { { -IN, H, IN }, { -IN, H, 1 }, { IN, YB, 1 }, { IN, YB, IN } }, { (2.0f * H) / SLANT, (2.0f - T) / SLANT, 0 }, COLOR_BAR_EDGE },
    { { { -IN, YA, IN }, { -IN, YA, 1 }, { IN, -H, 1 }, { IN, -H, IN } }, { -(2.0f * H) / SLANT, -(2.0f - T) / SLANT, 0 }, COLOR_BAR_EDGE },
    /* The bar from the inside, between the pillars. */
    { { { -IN, H, IN }, { IN, YB, IN }, { IN, -H, IN }, { -IN, YA, IN } }, { 0, 0, -1 }, COLOR_BAR_INSIDE },
    /* The outside: both pillars, then the bar across them. */
    { { { -1, -H, 1 }, { -IN, -H, 1 }, { -IN, H, 1 }, { -1, H, 1 } }, { 0, 0, 1 }, COLOR_PILLAR },
    { { { IN, -H, 1 }, { 1, -H, 1 }, { 1, H, 1 }, { IN, H, 1 } }, { 0, 0, 1 }, COLOR_PILLAR },
    { { { -1, H, 1 }, { -IN, H, 1 }, { 1, -H, 1 }, { IN, -H, 1 } }, { 0, 0, 1 }, COLOR_BAR },
};
#define SIDE_FACES  ((int) (sizeof(side) / sizeof(side[0])))

/* Where the light comes from: up, a little to the left, and toward us. */
static const vec_t light = { -0.37f, 0.46f, 0.80f };

static float turn_sin, turn_cos, tilt_sin, tilt_cos;
static float logo_x, logo_y, logo_size;

/* A point on side number `quarter`, turned and tilted into place. */
static vec_t place (vec_t p, int quarter) {
    /* Quarter turns need no sums: they just swap the two flat directions. */
    float x, z;
    switch (quarter & 3) {
        case 1: x = p.z; z = -p.x; break;
        case 2: x = -p.x; z = -p.z; break;
        case 3: x = -p.z; z = p.x; break;
        default: x = p.x; z = p.z; break;
    }
    vec_t v;
    float turned_z = (-x * turn_sin) + (z * turn_cos);
    v.x = (x * turn_cos) + (z * turn_sin);
    v.y = (p.y * tilt_cos) - (turned_z * tilt_sin);
    v.z = (p.y * tilt_sin) + (turned_z * tilt_cos);
    return v;
}

static void face_draw (const face_t *face, int quarter, logo_color_t color) {
    vec_t corner[4];
    vec_t middle = { 0, 0, 0 };
    for (int i = 0; i < 4; i++) {
        corner[i] = place(face->corner[i], quarter);
        middle.x += corner[i].x / 4;
        middle.y += corner[i].y / 4;
        middle.z += corner[i].z / 4;
    }

    /* A face looking away from us can't be seen. */
    vec_t facing = place(face->facing, quarter);
    if ((facing.x * -middle.x) + (facing.y * -middle.y) + (facing.z * (EYE - middle.z)) <= 0.0f) {
        return;
    }

    /* Faces turned toward the light are brighter, and each face is a
       little lighter at the top than at the bottom; the console blends the
       shades between the corners. A face turned almost exactly at the light
       catches a faint glint. (The first try glinted on every face that
       looked our way, which washed all the colors out to white.) */
    float lit = (facing.x * light.x) + (facing.y * light.y) + (facing.z * light.z);
    float diffuse = 0.60f + (0.40f * ((lit > 0.0f) ? lit : 0.0f));
    float shine = (lit > 0.0f) ? lit : 0.0f;
    shine = shine * shine;      /* raised to the 16th power */
    shine = shine * shine;
    shine = shine * shine;
    shine = shine * shine * 0.20f;

    sprite_t *wanted = NULL;
    if (color == COLOR_PILLAR) wanted = pattern[0];
    if (color == COLOR_PILLAR_2) wanted = pattern[1];

    color_t c = colors[color];
    float corners[4][9];
    for (int i = 0; i < 4; i++) {
        /* Things further away look smaller. */
        float distance = EYE - corner[i].z;
        float scale = (logo_size * EYE) / distance;
        corners[i][0] = logo_x + (corner[i].x * scale);
        corners[i][1] = logo_y - (corner[i].y * scale);

        float height = (face->corner[i].y + H) / (2.0f * H);    /* 0 at the bottom, 1 at the top */
        float shade = diffuse * (0.80f + (0.20f * height));
        float rgb[3] = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f };
        for (int k = 0; k < 3; k++) {
            float value = (rgb[k] * shade) + (wanted ? 0.0f : shine);
            corners[i][2 + k] = (value > 1.0f) ? 1.0f : value;
        }
        corners[i][5] = 1.0f;

        /* Where on the pattern this corner is. A pillar's outer face lists
           its corners bottom-left, bottom-right, top-right, top-left. */
        corners[i][6] = (wanted && (i == 1 || i == 2)) ? (float) wanted->width : 0.0f;
        corners[i][7] = (i < 2) ? (float) PATTERN_HEIGHT : 0.0f;
        corners[i][8] = 1.0f / distance;
    }

    if (wanted) {
        if (pattern_set != wanted) {
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
            rdpq_sprite_upload(TILE0, wanted, &(rdpq_texparms_t) { .s.repeats = REPEAT_INFINITE, .t.repeats = REPEAT_INFINITE });
            pattern_set = wanted;
        }
        rdpq_triangle(&TRIFMT_SHADE_TEX, corners[0], corners[1], corners[2]);
        rdpq_triangle(&TRIFMT_SHADE_TEX, corners[0], corners[2], corners[3]);
    } else {
        if (pattern_set) {
            rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
            pattern_set = NULL;
        }
        rdpq_triangle(&TRIFMT_SHADE, corners[0], corners[1], corners[2]);
        rdpq_triangle(&TRIFMT_SHADE, corners[0], corners[2], corners[3]);
    }
}

void intro_logo_open (void) {
    if (!pattern[0]) pattern[0] = sprite_load("rom:/logo_holo.sprite");
    if (!pattern[1]) pattern[1] = sprite_load("rom:/logo_stripes.sprite");
}

void intro_logo_close (void) {
    for (int i = 0; i < 2; i++) {
        if (pattern[i]) {
            sprite_free(pattern[i]);
            pattern[i] = NULL;
        }
    }
}

void intro_logo_draw (int center_x, int center_y, float size, float angle) {
    if (!pattern[0] || !pattern[1]) {
        return;     /* not opened */
    }
    float radians = angle * (3.14159265f / 180.0f);
    turn_sin = sinf(radians);
    turn_cos = cosf(radians);
    tilt_sin = sinf(TILT);
    tilt_cos = cosf(TILT);
    logo_x = center_x;
    logo_y = center_y;
    logo_size = size;

    /* The four sides, furthest first, so nearer ones cover them. */
    int order[4] = { 0, 1, 2, 3 };
    float depth[4];
    for (int i = 0; i < 4; i++) {
        depth[i] = place((vec_t) { 0, 0, 1.0f - (T / 2.0f) }, i).z;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (depth[order[j]] < depth[order[i]]) {
                int swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
        }
    }

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_persp(true);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
        pattern_set = NULL;

        for (int i = 0; i < 4; i++) {
            int quarter = order[i];
            for (int f = 0; f < SIDE_FACES; f++) {
                logo_color_t color = side[f].color;
                if (color == COLOR_PILLAR && (quarter & 1)) {
                    color = COLOR_PILLAR_2;
                }
                face_draw(&side[f], quarter, color);
            }
        }

        /* The tops of the four pillars go on last: nothing is above them. */
        static const face_t top = { { { -1, H, IN }, { -IN, H, IN }, { -IN, H, 1 }, { -1, H, 1 } }, { 0, 1, 0 }, COLOR_TOP };
        for (int quarter = 0; quarter < 4; quarter++) {
            face_draw(&top, quarter, COLOR_TOP);
        }
    rdpq_mode_pop();
}
