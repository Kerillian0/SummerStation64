#include <math.h>
#include <stdio.h>
#include <libdragon.h>

#include "fonts.h"
#include "games_ui.h"
#include "pad_test.h"
#include "sound.h"
#include "theme.h"
#include "ui_components/constants.h"

#define PORTS           (4)
#define LEAVE_MS        (2000)  /* hold B this long to leave */
#define RUMBLE_MS       (500)
#define USED_STICK      (25)    /* a stick pushed this far makes its controller the one shown */
#define VISITED         (30)    /* a direction counts once the stick has gone this far that way */

/* How far a new N64 stick reaches (libdragon: about 85 on good sticks, as
   low as 60 on worn ones); its gate's corners, about 70 each way. */
#define REACH_STRAIGHT  (85.0f)
#define REACH_CORNER    (99.0f)
#define LIKE_NEW        (90)    /* percent of a new stick's reach */
#define WORN            (72)

/* Where things are. */
#define TITLE_X         (48)
#define TITLE_Y         (28)
#define PORTS_X         (420)
#define PORT_W          (36)
#define PORT_H          (26)
#define PORT_GAP        (8)
#define PANEL_Y0        (68)
#define PANEL_Y1        (392)
#define LEFT_X0         (40)
#define LEFT_X1         (382)
#define RIGHT_X0        (394)
#define RIGHT_X1        (600)
#define PAD_CX          (211)   /* the controller's middle */
#define PAD_TOP         (124)
#define PLOT_CX         ((RIGHT_X0 + RIGHT_X1) / 2)
#define PLOT_CY         (184)
#define PLOT_SCALE      (0.95f) /* pixels per stick unit */
#define HINTS_Y         (404)

static bool active = false;
static int shown = 0;                   /* the port drawn */
static float reach[PORTS][8];           /* furthest the stick went, per direction (0 = right, counted anticlockwise) */
static uint64_t b_since = 0;            /* B held since, 0 when it isn't */
static bool b_was_down = false;
static uint64_t rumble_until = 0;
static bool both_were_down = false;     /* Z + A, so one press rumbles once */

void pad_test_open (void) {
    active = true;
    for (int p = 0; p < PORTS; p++) {
        for (int d = 0; d < 8; d++) {
            reach[p][d] = 0.0f;
        }
    }
    shown = 0;
    for (int p = 0; p < PORTS; p++) {
        if (joypad_is_connected(p)) {
            shown = p;
            break;
        }
    }
    b_since = 0;
    b_was_down = true;      /* a B already down (none should be) isn't a hold */
    both_were_down = true;
    rumble_until = 0;
}

bool pad_test_active (void) {
    return active;
}

static void pad_rumble_off (void) {
    if (rumble_until) {
        rumble_until = 0;
        for (int p = 0; p < PORTS; p++) {
            if (joypad_is_connected(p) && joypad_get_rumble_supported(p)) {
                joypad_set_rumble_active(p, false);
            }
        }
    }
}

static void note_stick (int port, int x, int y) {
    float m = sqrtf((float) (x * x + y * y));
    if (m < 8.0f) {
        return;
    }
    float a = atan2f((float) y, (float) x);     /* up is positive on the stick */
    int d = ((int) lroundf(a / ((float) M_PI / 4.0f)) + 8) % 8;
    if (m > reach[port][d]) {
        reach[port][d] = m;
    }
}

void pad_test_process (menu_t *menu) {
    (void) menu;    /* every button is read straight from the controllers */
    uint64_t now = get_ticks_ms();
    bool b_down = false;

    if (!joypad_is_connected(shown)) {
        for (int p = 0; p < PORTS; p++) {
            if (joypad_is_connected(p)) {
                shown = p;
                break;
            }
        }
    }
    for (int p = 0; p < PORTS; p++) {
        if (!joypad_is_connected(p)) {
            continue;
        }
        joypad_inputs_t in = joypad_get_inputs(p);
        note_stick(p, in.stick_x, in.stick_y);
        bool used = in.btn.raw != 0 || in.stick_x > USED_STICK || in.stick_x < -USED_STICK
            || in.stick_y > USED_STICK || in.stick_y < -USED_STICK;
        if (used && p != shown) {
            shown = p;      /* whichever was last used */
        }
        b_down = b_down || in.btn.b;
    }

    if (b_down && !b_was_down) {
        b_since = now;
    } else if (!b_down) {
        b_since = 0;
    }
    b_was_down = b_down;
    if (b_since && now - b_since >= LEAVE_MS) {
        pad_rumble_off();
        active = false;
        sound_play_effect(SFX_EXIT);
        return;
    }

    joypad_buttons_t btn = joypad_get_buttons(shown);
    bool both = btn.z && btn.a;
    if (both && !both_were_down && joypad_get_rumble_supported(shown)) {
        joypad_set_rumble_active(shown, true);
        rumble_until = now + RUMBLE_MS;
    }
    both_were_down = both;
    if (rumble_until && now >= rumble_until) {
        pad_rumble_off();
    }
}

/* ---------- drawing ---------- */

static void flat (color_t color) {
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    if (color.a < 255) {
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    }
    rdpq_set_prim_color(color);
}

static void disc (float cx, float cy, float r, color_t color) {
    flat(color);
    const int segs = 20;
    for (int i = 0; i < segs; i++) {
        float a0 = (2.0f * (float) M_PI * i) / segs, a1 = (2.0f * (float) M_PI * (i + 1)) / segs;
        float c[2] = { cx, cy };
        float p0[2] = { cx + r * cosf(a0), cy + r * sinf(a0) };
        float p1[2] = { cx + r * cosf(a1), cy + r * sinf(a1) };
        rdpq_triangle(&TRIFMT_FILL, c, p0, p1);
    }
}

/* A ring from straight up, clockwise, `part` of the way round (the hold-B ring). */
static void arc (float cx, float cy, float r, float width, float part, color_t color) {
    flat(color);
    const int segs = 32;
    int n = (int) ceilf(segs * part);
    float r0 = r - (width / 2.0f), r1 = r + (width / 2.0f);
    for (int i = 0; i < n; i++) {
        float t0 = (float) i / segs, t1 = (float) (i + 1) / segs;
        t1 = (t1 > part) ? part : t1;
        float a0 = (2.0f * (float) M_PI * t0) - ((float) M_PI / 2.0f);
        float a1 = (2.0f * (float) M_PI * t1) - ((float) M_PI / 2.0f);
        float i0[2] = { cx + r0 * cosf(a0), cy + r0 * sinf(a0) };
        float i1[2] = { cx + r0 * cosf(a1), cy + r0 * sinf(a1) };
        float o0[2] = { cx + r1 * cosf(a0), cy + r1 * sinf(a0) };
        float o1[2] = { cx + r1 * cosf(a1), cy + r1 * sinf(a1) };
        rdpq_triangle(&TRIFMT_FILL, i0, o0, o1);
        rdpq_triangle(&TRIFMT_FILL, i0, o1, i1);
    }
}

/* A thick line from one point to another (the handles, the plot's outlines). */
static void bar (float x0, float y0, float x1, float y1, float width, color_t color) {
    flat(color);
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f) {
        return;
    }
    float nx = -dy / len * (width / 2.0f), ny = dx / len * (width / 2.0f);
    float a[2] = { x0 + nx, y0 + ny }, b[2] = { x1 + nx, y1 + ny };
    float c[2] = { x1 - nx, y1 - ny }, d[2] = { x0 - nx, y0 - ny };
    rdpq_triangle(&TRIFMT_FILL, a, b, c);
    rdpq_triangle(&TRIFMT_FILL, a, c, d);
}

static void rect (int x0, int y0, int x1, int y1, color_t color) {
    flat(color);
    rdpq_fill_rectangle(x0, y0, x1, y1);
}

static void text (int x, int y, int w, int h, rdpq_align_t align, menu_font_style_t style, const char *s) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = h, .align = align, .valign = VALIGN_CENTER,
        .style_id = style }, FNT_DEFAULT, x, y, "%s", s);
}

static color_t dim (color_t c, bool lit) {
    if (lit) {
        return c;
    }
    return RGBA32((c.r * 2) / 5, (c.g * 2) / 5, (c.b * 2) / 5, 0xFF);
}

/* A round button: a little larger and full color while pressed. */
static void button (float cx, float cy, float r, color_t color, bool down, const char *label) {
    if (down) {
        disc(cx, cy, r + 3.0f, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    }
    disc(cx, cy, r + (down ? 1.0f : 0.0f), dim(color, down));
    if (label) {
        text((int) cx - 14, (int) cy - 10, 28, 20, ALIGN_CENTER, STL_WHITE, label);
    }
}

/* L, R and Z: a rounded bar with its letter. */
static void shoulder (int x0, int y0, int x1, int y1, bool down, const char *label) {
    const theme_t *t = theme_get();
    games_ui_round_fill(x0, y0, x1, y1, down ? t->accent : RGBA32(0x50, 0x50, 0x56, 0xFF));
    text(x0, y0, x1 - x0, y1 - y0, ALIGN_CENTER, down ? STL_WHITE : STL_GRAY, label);
}

static void draw_controller (joypad_inputs_t in, bool connected) {
    const float cx = PAD_CX, top = PAD_TOP;
    color_t body = connected ? RGBA32(0x9C, 0x9C, 0xA4, 0xFF) : RGBA32(0x5A, 0x5A, 0x60, 0xFF);
    color_t well = RGBA32(0x3C, 0x3C, 0x42, 0xFF);
    joypad_buttons_t b = in.btn;
    if (!connected) {
        b.raw = 0;
        in.stick_x = in.stick_y = 0;
    }

    /* L and R first, so the body hides their lower half. */
    shoulder((int) cx - 162, (int) top - 22, (int) cx - 92, (int) top + 10, b.l, "L");
    shoulder((int) cx + 92, (int) top - 22, (int) cx + 162, (int) top + 10, b.r, "R");

    /* The body and its three handles. */
    bar(cx - 128, top + 50, cx - 136, top + 178, 58, body);
    disc(cx - 136, top + 178, 29, body);
    bar(cx + 128, top + 50, cx + 136, top + 178, 58, body);
    disc(cx + 136, top + 178, 29, body);
    bar(cx, top + 50, cx, top + 168, 62, body);
    disc(cx, top + 168, 31, body);
    games_ui_round_fill((int) cx - 166, (int) top, (int) cx + 166, (int) top + 86, body);

    /* D-Pad. */
    const float dx = cx - 122, dy = top + 42, arm = 17, w = 15;
    color_t pad_off = RGBA32(0x34, 0x34, 0x38, 0xFF), pad_on = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
    rect((int) (dx - w / 2), (int) (dy - w / 2), (int) (dx + w / 2), (int) (dy + w / 2), pad_off);
    rect((int) (dx - w / 2), (int) (dy - w / 2 - arm), (int) (dx + w / 2), (int) (dy - w / 2), b.d_up ? pad_on : pad_off);
    rect((int) (dx - w / 2), (int) (dy + w / 2), (int) (dx + w / 2), (int) (dy + w / 2 + arm), b.d_down ? pad_on : pad_off);
    rect((int) (dx - w / 2 - arm), (int) (dy - w / 2), (int) (dx - w / 2), (int) (dy + w / 2), b.d_left ? pad_on : pad_off);
    rect((int) (dx + w / 2), (int) (dy - w / 2), (int) (dx + w / 2 + arm), (int) (dy + w / 2), b.d_right ? pad_on : pad_off);

    /* START, B, A, the C buttons. */
    button(cx, top + 42, 11, RGBA32(0xE0, 0x30, 0x30, 0xFF), b.start, NULL);
    button(cx + 70, top + 38, 13, RGBA32(0x30, 0xB0, 0x40, 0xFF), b.b, "B");
    button(cx + 96, top + 62, 15, RGBA32(0x30, 0x60, 0xE0, 0xFF), b.a, "A");
    color_t yellow = RGBA32(0xF0, 0xC8, 0x20, 0xFF);
    const float ccx = cx + 132, ccy = top + 36, cs = 17, cr = 9;
    button(ccx, ccy - cs, cr, yellow, b.c_up, NULL);
    button(ccx, ccy + cs, cr, yellow, b.c_down, NULL);
    button(ccx - cs, ccy, cr, yellow, b.c_left, NULL);
    button(ccx + cs, ccy, cr, yellow, b.c_right, NULL);

    /* The stick in its gate. */
    const float sx = cx, sy = top + 118, gate = 30;
    disc(sx, sy, gate, well);
    float kx = sx + (in.stick_x / REACH_STRAIGHT) * (gate - 12);
    float ky = sy - (in.stick_y / REACH_STRAIGHT) * (gate - 12);
    disc(kx, ky, 13, connected ? RGBA32(0xD8, 0xD8, 0xDC, 0xFF) : body);

    /* Z sits under the middle handle. */
    shoulder((int) cx - 26, (int) top + 210, (int) cx + 26, (int) top + 236, b.z, "Z");
}

/* The reach a new stick has in direction d. */
static float new_reach (int d) {
    return (d % 2) ? REACH_CORNER : REACH_STRAIGHT;
}

static void plot_point (int d, float r, float *x, float *y) {
    float a = (float) d * ((float) M_PI / 4.0f);
    *x = PLOT_CX + cosf(a) * r * PLOT_SCALE;
    *y = PLOT_CY - sinf(a) * r * PLOT_SCALE;
}

static void draw_range (joypad_inputs_t in, bool connected) {
    const float *got = reach[shown];
    int visited = 0;
    float sum = 0.0f;
    for (int d = 0; d < 8; d++) {
        if (got[d] >= VISITED) {
            visited++;
        }
        float k = got[d] / new_reach(d);
        sum += (k > 1.0f) ? 1.0f : k;
    }
    int percent = (int) lroundf((sum * 100.0f) / 8.0f);
    color_t rating = (percent >= LIKE_NEW) ? RGBA32(0x40, 0xD0, 0x50, 0xFF)
        : (percent >= WORN) ? RGBA32(0xF0, 0xA0, 0x30, 0xFF) : RGBA32(0xF0, 0x40, 0x40, 0xFF);

    /* Axes, then the shape the stick has reached, then a new stick's. */
    color_t faint = RGBA32(0x60, 0x60, 0x68, 0xFF);
    float half = REACH_CORNER * PLOT_SCALE * 0.75f;
    bar(PLOT_CX - half - 8, PLOT_CY, PLOT_CX + half + 8, PLOT_CY, 2, faint);
    bar(PLOT_CX, PLOT_CY - half - 8, PLOT_CX, PLOT_CY + half + 8, 2, faint);
    flat(RGBA32(rating.r, rating.g, rating.b, 0x90));
    for (int d = 0; d < 8; d++) {
        float c[2] = { PLOT_CX, PLOT_CY }, p0[2], p1[2];
        plot_point(d, got[d], &p0[0], &p0[1]);
        plot_point((d + 1) % 8, got[(d + 1) % 8], &p1[0], &p1[1]);
        rdpq_triangle(&TRIFMT_FILL, c, p0, p1);
    }
    for (int d = 0; d < 8; d++) {
        float x0, y0, x1, y1;
        plot_point(d, new_reach(d), &x0, &y0);
        plot_point((d + 1) % 8, new_reach((d + 1) % 8), &x1, &y1);
        bar(x0, y0, x1, y1, 2, RGBA32(0xC8, 0xC8, 0xD0, 0xFF));
    }
    if (connected) {
        disc(PLOT_CX + in.stick_x * PLOT_SCALE, PLOT_CY - in.stick_y * PLOT_SCALE, 5, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    }

    char line[48];
    int w = RIGHT_X1 - RIGHT_X0;
    snprintf(line, sizeof(line), "X %+4d    Y %+4d", connected ? in.stick_x : 0, connected ? in.stick_y : 0);
    text(RIGHT_X0, 286, w, 20, ALIGN_CENTER, STL_DEFAULT, line);
    if (visited < 8) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = w - 20, .height = 44, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
            .wrap = WRAP_WORD, .style_id = STL_GRAY }, FNT_DEFAULT, RIGHT_X0 + 10, 310, "Roll the stick round its edge a few times.");
        return;
    }
    snprintf(line, sizeof(line), "Range %d%%", percent);
    text(RIGHT_X0, 310, w, 20, ALIGN_CENTER, STL_DEFAULT, line);
    const char *word = (percent >= LIKE_NEW) ? "Like new" : (percent >= WORN) ? "Worn" : "Very worn";
    menu_font_style_t style = (percent >= LIKE_NEW) ? STL_GREEN : (percent >= WORN) ? STL_ORANGE : STL_RED;
    rdpq_text_printf(&(rdpq_textparms_t) { .width = w, .height = 32, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .style_id = style }, FNT_TITLE_MEDIUM, RIGHT_X0, 334, "%s", word);
}

static const char *accessory_name (int port) {
    if (!joypad_is_connected(port)) {
        return "No controller";
    }
    switch (joypad_get_accessory_type(port)) {
        case JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK: return "Rumble Pak";
        case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK: return "Controller Pak";
        case JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK: return "Transfer Pak";
        case JOYPAD_ACCESSORY_TYPE_BIO_SENSOR: return "Bio Sensor";
        case JOYPAD_ACCESSORY_TYPE_SNAP_STATION: return "Snap Station";
        case JOYPAD_ACCESSORY_TYPE_NONE: return "Nothing plugged in";
        default: return "Unknown accessory";
    }
}

static void draw_ports (void) {
    const theme_t *t = theme_get();
    for (int p = 0; p < PORTS; p++) {
        int x = PORTS_X + p * (PORT_W + PORT_GAP), y = TITLE_Y + 4;
        bool in = joypad_is_connected(p);
        if (p == shown) {
            games_ui_round_fill(x - 3, y - 3, x + PORT_W + 3, y + PORT_H + 3, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
        }
        games_ui_round_fill(x, y, x + PORT_W, y + PORT_H, in ? t->accent : t->tab_inactive);
        char label[2] = { (char) ('1' + p), 0 };
        text(x, y, PORT_W, PORT_H, ALIGN_CENTER, in ? STL_WHITE : STL_GRAY, label);
    }
}

void pad_test_draw (void) {
    const theme_t *t = theme_get();
    bool connected = joypad_is_connected(shown);
    joypad_inputs_t in = connected ? joypad_get_inputs(shown) : (joypad_inputs_t) { 0 };

    rdpq_text_printf(&(rdpq_textparms_t) { .width = 360, .height = 34, .valign = VALIGN_CENTER },
        FNT_TITLE, TITLE_X, TITLE_Y, "Controller Test");
    draw_ports();

    color_t panel = RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8);
    games_ui_round_fill(LEFT_X0, PANEL_Y0, LEFT_X1, PANEL_Y1, panel);
    games_ui_round_fill(RIGHT_X0, PANEL_Y0, RIGHT_X1, PANEL_Y1, panel);

    char line[48];
    snprintf(line, sizeof(line), "Port %d: %s", shown + 1, accessory_name(shown));
    text(LEFT_X0, PANEL_Y0 + 8, LEFT_X1 - LEFT_X0, 22, ALIGN_CENTER, STL_DEFAULT, line);
    text(RIGHT_X0, PANEL_Y0 + 8, RIGHT_X1 - RIGHT_X0, 22, ALIGN_CENTER, STL_DEFAULT, "Stick range");

    rdpq_mode_push();
    draw_controller(in, connected);
    draw_range(in, connected);
    rdpq_mode_pop();

    /* Hints: the ring fills while B is held. */
    games_ui_round_fill(LEFT_X0, HINTS_Y, RIGHT_X1, HINTS_Y + 32, panel);
    float part = b_since ? (float) (get_ticks_ms() - b_since) / LEAVE_MS : 0.0f;
    part = (part > 1.0f) ? 1.0f : part;
    rdpq_mode_push();
    disc(LEFT_X0 + 24, HINTS_Y + 16, 11, RGBA32(0x30, 0xB0, 0x40, 0xFF));
    if (part > 0.0f) {
        arc(LEFT_X0 + 24, HINTS_Y + 16, 13.5f, 3.0f, part, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    }
    rdpq_mode_pop();
    text(LEFT_X0 + 14, HINTS_Y + 6, 20, 20, ALIGN_CENTER, STL_WHITE, "B");
    text(LEFT_X0 + 44, HINTS_Y + 6, 200, 20, ALIGN_LEFT, STL_DEFAULT, "Hold to leave");
    if (connected && joypad_get_rumble_supported(shown)) {
        text(RIGHT_X1 - 220, HINTS_Y + 6, 204, 20, ALIGN_RIGHT, STL_DEFAULT, "Z + A  Rumble");
    }
}
