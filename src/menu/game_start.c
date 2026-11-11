#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "art_tint.h"
#include "baked_art.h"
#include "box3d.h"
#include "frame_rate.h"
#include "game_info_ui.h"
#include "game_start.h"
#include "menu_options.h"
#include "sound.h"
#include "theme.h"
#include "ui_components/constants.h"
#include "video_preview.h"
#include "volume.h"

/* The opening, on Game info's own frames (the music fades meanwhile). */
#define FLY_MS          (280)   /* the cover flies to the middle, becoming a box; the page fades into its backdrop */
#define FLAP_MS         (240)   /* the top flap swings open, a little past, and settles */
#define RISE_MS         (360)   /* the cartridge pops up out of the box */
#define OPENING_MS      (FLY_MS + FLAP_MS + RISE_MS)
#define MUSIC_GONE_MS   (OPENING_MS - 150)  /* silent a few frames before loading, so the sound chip's buffers hold silence */

/* While the game loads. The animation keeps its own time; the part before
   the click (the spin and the label) slows down to last as long as the
   loading is expected to, and the click waits for the loading to finish. */
#define AWAY_MS         (320)   /* the box drops away */
#define MOVE_MS         (600)   /* the cartridge goes to the middle of the screen, growing... */
#define SPIN_MS         (850)   /* ...spinning twice, slowing to face the front */
#define LABEL_MS        (320)   /* the label shows */
#define BEFORE_CLICK_MS (SPIN_MS + LABEL_MS)
#define WINDUP_MS       (110)   /* it lifts a little... */
#define SLAM_MS         (140)   /* ...and snaps down from the middle into the bottom of the screen, a little too far */
#define BOUNCE_MS       (180)   /* and springs back */
#define REST_MS         (160)
#define FADE_MS         (260)   /* to black */

/* The scene: one space for the box and the cartridge (box3d.h,
   pose->world), in screen pixels at its middle's depth (about a millimetre). */
#define CAMERA          (840.0f)
#define ORIGIN_X        (320.0f)
#define ORIGIN_Y        (236.0f)
#define BOX_LONG        (230.0f)    /* the box's longest side */
#define YAW             (14.0f)     /* turned a little, so its side shows */
#define PITCH           (16.0f)     /* seen from a little above, into its top */
#define CART_SHARE      (0.5f)      /* the cartridge's width against the box's longest side, in the box */
#define CART_GROW       (1.35f)     /* and how much bigger it grows once out, on its way to the middle */
#define HOVER_MIDDLE    (240.0f)    /* while it spins and shows its label: its middle on this screen line */
#define HOME_TOP        (416.0f)    /* clicked in: its top this far down the screen, the rest below the edge */
#define STAGE_BRIGHT    (0x58)      /* the backdrop as Game info draws it */
#define CART_MIN_FREE   (160 * 1024)
#define THUNK_PATH      "rom:/thunk.wav64"  /* assets/sounds/make_thunk.py */
/* The back every cartridge shares, its grey plastic colored to match the front (as on the cover rows). */
#define CART_BACK_PLASTIC   (176)
#define CART_LABEL_X0       (0.14f)
#define CART_LABEL_X1       (0.86f)
#define CART_LABEL_Y0       (0.28f)
#define CART_LABEL_Y1       (0.84f)


static enum { IDLE, OPENING, LOADING } stage = IDLE;
static menu_t *menu_now = NULL;
static uint64_t began = 0;          /* A was pressed */
static uint64_t load_began = 0;     /* the copying started */
static uint64_t last_frame = 0;
static float load_progress = 0.0f;
static bool load_done = false;
static float before_click = 0.0f;   /* the animation's own time through the spin and the label (ms) */
static uint64_t click_at = 0;       /* the wind-up began */

static surface_t *front = NULL;     /* the cover (Game info's: not ours to free) */
static component_boxart_t *top_art = NULL;
static component_boxart_t *cart_art = NULL;
static sprite_t *back_sprite = NULL;    /* the back every cartridge shares (and the front of one without a picture) */
static surface_t back_surface;
static sprite_t *pcb = NULL;
static wav64_t *thunk = NULL;       /* the cartridge going home, held in memory (rom:/ is gone by then) */
static bool thunked = false;
static surface_t pcb_surface;
static color_t box_side, cart_body;
static float from_x, from_y, from_w;    /* the cover's place on Game info */
static float box_w, box_h, cart_w, cart_h;

const char *game_start_name (int choice) {
    static const char *names[GAME_START_COUNT] = { "Box Opens", "Loading Bar" };
    return (choice >= 0 && choice < GAME_START_COUNT) ? names[choice] : "";
}

static bool memory_free_for (int bytes) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    return (heap.total - heap.used) >= bytes;
}

static bool file_there (const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

/* sprite_load stops the menu when the file is missing or memory runs out: check first. */
static sprite_t *sprite_from_rom (const char *path, surface_t *surface) {
    if (!memory_free_for(CART_MIN_FREE) || !file_there(path)) {
        return NULL;
    }
    sprite_t *sprite = sprite_load(path);
    if (sprite) {
        *surface = sprite_get_pixels(sprite);
    }
    return sprite;
}

static surface_t *cart_front (void) {
    if (cart_art && cart_art->image) {
        return cart_art->image;
    }
    return back_sprite ? &back_surface : NULL;
}

/* The plastic's color, from the picture's left grip (as the cover rows do). */
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

/* The shared back's grey plastic in this cartridge's color; its label keeps its own. */
static void back_tint (color_t body) {
    surface_t *image = &back_surface;
    if (surface_get_format(image) != FMT_RGBA16) {
        return;
    }
    int tr = body.r * 255 / CART_BACK_PLASTIC, tg = body.g * 255 / CART_BACK_PLASTIC, tb = body.b * 255 / CART_BACK_PLASTIC;
    tr = (tr > 255) ? 255 : tr; tg = (tg > 255) ? 255 : tg; tb = (tb > 255) ? 255 : tb;
    int w = image->width, h = image->height;
    int lx0 = (int) (w * CART_LABEL_X0), lx1 = (int) (w * CART_LABEL_X1);
    int ly0 = (int) (h * CART_LABEL_Y0), ly1 = (int) (h * CART_LABEL_Y1);
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

static void free_all (void) {
    rspq_wait();    /* nothing may still be drawing from them */
    if (top_art) {
        baked_art_free(top_art);
        top_art = NULL;
    }
    if (cart_art) {
        baked_art_free(cart_art);
        cart_art = NULL;
    }
    if (back_sprite) {
        sprite_free(back_sprite);
        back_sprite = NULL;
    }
    if (pcb) {
        sprite_free(pcb);
        pcb = NULL;
    }
    if (thunk) {
        mixer_ch_stop(SOUND_SFX_CHANNEL);
        wav64_close(thunk);
        thunk = NULL;
    }
    front = NULL;
}

bool game_start_begin (menu_t *menu, component_boxart_t *cover) {
    if (options_get(OPTION_GAME_START) != GAME_START_BOX) {
        return false;
    }
    free_all();
    menu_now = menu;
    video_preview_stop();   /* the cover, not its clip, goes into the box */
    const char *code = menu->load.rom_info.game_code;
    front = (cover && !cover->loading && cover->image && cover->image->width > 0 && cover->image->height > 0) ? cover->image : NULL;
    if (front) {
        float longest = (front->width > front->height) ? front->width : front->height;
        box_w = BOX_LONG * front->width / longest;
        box_h = BOX_LONG * front->height / longest;
        if (!art_tint_from(front, &box_side)) {
            box_side = theme_get()->panel;
        }
        if (baked_art_has(code, IMAGE_BOXART_TOP)) {
            top_art = baked_art_load(code, IMAGE_BOXART_TOP);
        }
    } else {
        box_w = BOX_LONG;
        box_h = BOX_LONG * 0.7f;
    }
    if (baked_art_has(code, IMAGE_GAMEPAK_FRONT)) {
        cart_art = baked_art_load(code, IMAGE_GAMEPAK_FRONT);
    }
    back_sprite = sprite_from_rom("rom:/cart_back.sprite", &back_surface);
    pcb = sprite_from_rom("rom:/cart_bottom.sprite", &pcb_surface);
    if (menu->settings.soundfx_enabled && memory_free_for(CART_MIN_FREE) && file_there(THUNK_PATH)) {
        thunk = wav64_load(THUNK_PATH, &(wav64_loadparms_t) { .streaming_mode = WAV64_STREAMING_NONE });     /* whole, into memory */
    }
    thunked = false;
    surface_t *cart = cart_front();
    cart_body = plastic_of(cart);
    if (back_sprite && cart_art && cart_art->image) {
        back_tint(cart_body);   /* a plain grey cartridge keeps its grey */
    }
    cart_w = BOX_LONG * CART_SHARE;
    cart_h = cart ? cart_w * cart->height / cart->width : cart_w * 0.65f;

    int rect[4];
    if (game_info_ui_cover_rect(rect)) {
        from_x = (rect[0] + rect[2]) / 2.0f;
        from_y = (rect[1] + rect[3]) / 2.0f;
        from_w = rect[2] - rect[0];
    } else {
        from_x = ORIGIN_X;
        from_y = ORIGIN_Y;
        from_w = box_w;
    }
    game_info_ui_hide_cover(true);
    began = get_ticks_ms();
    load_progress = 0.0f;
    load_done = false;
    before_click = 0.0f;
    click_at = 0;
    stage = OPENING;
    debugf("game start: box %s, top %s, cartridge %s\n", front ? "yes" : "no", top_art ? "yes" : "no",
        cart_art ? "its own" : (back_sprite ? "plain" : "none"));
    return true;
}

bool game_start_opening (void) {
    return stage == OPENING;
}

bool game_start_ready (void) {
    return stage == OPENING && get_ticks_ms() - began >= OPENING_MS;
}

bool game_start_page_gone (void) {
    return stage == LOADING || (stage == OPENING && get_ticks_ms() - began >= FLY_MS);
}

/* ---------- the scene ---------- */

static float clamp01 (float t) {
    return (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;
}

static float smooth (float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

static float mixf (float a, float b, float t) {
    return a + (b - a) * t;
}

static void box_draw (box3d_pose_t pose, float open, int parts, int light, int alpha) {
    if (!front) {
        return;
    }
    box3d_art_t art = {
        .front = front,
        .top = (top_art && top_art->image) ? top_art->image : NULL,
        .side = box_side,
        .bright = light,
        .alpha = alpha,
    };
    pose.open = open;
    pose.parts = parts;
    box3d_draw(&art, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, &pose);
}

/* The cartridge at (0, y, z) turned `turn` degrees; past a quarter turn its back shows. */
static void cart_draw (box3d_pose_t pose, float y, float z, float turn, int light, int alpha, float size) {
    surface_t *picture = cart_front();
    if (!picture) {
        return;
    }
    turn = fmodf(turn, 360.0f);
    turn = (turn > 180.0f) ? turn - 360.0f : (turn < -180.0f) ? turn + 360.0f : turn;
    if (turn > 90.0f || turn < -90.0f) {
        picture = back_sprite ? &back_surface : picture;
        turn += (turn > 0.0f) ? -180.0f : 180.0f;
    }
    box3d_art_t art = {
        .front = picture,
        .side = cart_body,
        .bright = light,
        .alpha = alpha,
        .cartridge = true,
        .bottom = pcb ? &pcb_surface : NULL,
    };
    pose.width = cart_w * size;
    pose.yaw = turn;
    pose.shift_x = 0.0f;
    pose.shift_y = y;
    pose.shift_z = z;
    pose.open = 0.0f;
    pose.parts = 0;
    box3d_draw(&art, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, &pose);
}

/* The stage behind it all: the game's blurred backdrop, or black. */
static void stage_draw (menu_t *menu, int light, int alpha) {
    if (game_info_ui_backdrop_draw(menu, STAGE_BRIGHT * light / 255, alpha)) {
        return;
    }
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_set_prim_color(RGBA32(0, 0, 0, alpha));
        if (alpha < 255) {
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    rdpq_mode_pop();
}

static void opening_draw (menu_t *menu, float t) {
    float inside_y = box_h / 2 - cart_h / 2 - 3.0f;         /* standing on the box's floor */
    float out_y = -box_h / 2 - cart_h / 2 - 8.0f;           /* lifted clear of its top */
    float fly = clamp01(t / FLY_MS);
    float e = animations_ease_out(fly);
    float music = 1.0f - clamp01(t / MUSIC_GONE_MS);
    mixer_ch_set_vol(SOUND_BGM_CHANNEL, volume_music() * music, volume_music() * music);

    stage_draw(menu, 255, (fly < 1.0f) ? (int) (255 * fly) : 255);     /* the page fades into the backdrop */
    box3d_pose_t pose = {
        .world = true, .camera = CAMERA, .amount = e, .yaw = YAW, .pitch = PITCH,
        .width = mixf(from_w, box_w, e), .cx = mixf(from_x, ORIGIN_X, e), .cy = mixf(from_y, ORIGIN_Y, e),
    };
    float open_t = clamp01((t - FLY_MS) / FLAP_MS);
    float open = (open_t > 0.0f) ? animations_ease_back(open_t) : 0.0f;
    float rise_t = clamp01((t - FLY_MS - FLAP_MS) / RISE_MS);
    float rise = (rise_t > 0.0f) ? animations_ease_back(rise_t) : 0.0f;
    if (!front) {
        cart_draw(pose, out_y, 0.0f, YAW, 255, (int) (255 * fly), 1.0f);     /* no cover: the cartridge just appears */
        return;
    }
    box_draw(pose, open, BOX3D_PART_BACK, 255, 255);
    if (open > 0.0f) {
        cart_draw(pose, mixf(inside_y, out_y, rise), 0.0f, YAW, 255, 255, 1.0f);
    }
    box_draw(pose, open, BOX3D_PART_FRONT, 255, 255);
}

/* The height in the scene that shows on screen line `line` (at the middle's depth). */
static float scene_y_for (float line) {
    float tip = PITCH * (float) M_PI / 180.0f;
    float d = line - ORIGIN_Y;
    return d * CAMERA / (cosf(tip) * CAMERA - sinf(tip) * d);
}

static void loading_draw (menu_t *menu, uint64_t now) {
    float out_y = -box_h / 2 - cart_h / 2 - 8.0f;
    float home_y = scene_y_for(HOME_TOP) + cart_h * CART_GROW / 2;
    float hover_y = scene_y_for(HOVER_MIDDLE);
    float t = (float) (now - load_began);

    /* The animation's own clock up to the click: slowed so it lasts as long
       as the loading is expected to (from how far it has got so far). */
    float dt = (float) (now - last_frame);
    last_frame = now;
    float rate = 1.0f;
    float left = BEFORE_CLICK_MS - before_click;
    if (!load_done && load_progress >= 0.02f && left > 0.0f) {
        float expected = t / load_progress - t;     /* loading time still to come */
        if (expected > left) {
            rate = left / expected;
        }
    }
    before_click += dt * rate;
    if (before_click >= BEFORE_CLICK_MS) {
        before_click = BEFORE_CLICK_MS;
        if (load_done && !click_at) {
            click_at = now;
        }
    }

    float away = clamp01(t / AWAY_MS);
    box3d_pose_t pose = {
        .world = true, .camera = CAMERA, .cx = ORIGIN_X, .cy = ORIGIN_Y, .amount = 1.0f,
        .width = box_w, .yaw = YAW * (1.0f - animations_ease_out(away)), .pitch = PITCH,
    };

    /* The cartridge: growing as it goes to the middle of the screen, spinning to a
       stop on its label, a little bob; then the wind-up, the snap, the
       spring back. */
    float move = smooth(before_click / MOVE_MS);
    float spin = clamp01(before_click / SPIN_MS);
    float turn = YAW + (720.0f - YAW) * animations_ease_out(spin);
    float cart_y = mixf(out_y, hover_y, move);
    float label = clamp01((before_click - SPIN_MS) / LABEL_MS);
    cart_y -= 4.0f * sinf(label * (float) M_PI);
    int light = 255;
    if (click_at) {
        float c = (float) (now - click_at);
        float windup = clamp01(c / WINDUP_MS);
        float slam = clamp01((c - WINDUP_MS) / SLAM_MS);
        float bounce = clamp01((c - WINDUP_MS - SLAM_MS) / BOUNCE_MS);
        float lifted = hover_y - 16.0f * animations_ease_out(windup);
        if (slam <= 0.0f) {
            cart_y = lifted;
        } else if (bounce <= 0.0f) {
            cart_y = mixf(lifted, home_y + 9.0f, slam * slam);   /* faster and faster */
        } else {
            if (!thunked && thunk) {
                thunked = true;     /* it lands: the soft thunk */
                mixer_ch_set_vol(SOUND_SFX_CHANNEL, volume_effects(), volume_effects());
                wav64_play(thunk, SOUND_SFX_CHANNEL);
            }
            float k = 1.0f - bounce;
            cart_y = home_y + 9.0f * k * cosf(3.0f * (float) M_PI * bounce);
        }
        float fade = clamp01((c - WINDUP_MS - SLAM_MS - BOUNCE_MS - REST_MS) / FADE_MS);
        light = (int) (255 * (1.0f - fade));
    }

    stage_draw(menu, light, 255);
    if (away < 1.0f) {
        box3d_pose_t falling = pose;
        falling.shift_y = 300.0f * away * away;
        box_draw(falling, 1.0f, 0, light, (int) (255 * (1.0f - away)));
    }
    cart_draw(pose, cart_y, 0.0f, turn, light, 255, mixf(1.0f, CART_GROW, move));
}

static bool finished (void) {
    return click_at && get_ticks_ms() - click_at >= WINDUP_MS + SLAM_MS + BOUNCE_MS + REST_MS + FADE_MS;
}

void game_start_draw (menu_t *menu) {
    if (stage == OPENING) {
        opening_draw(menu, (float) (get_ticks_ms() - began));
    } else if (stage == LOADING) {
        loading_draw(menu, get_ticks_ms());
    }
}

/* ---------- while the game loads ---------- */

static void frame (surface_t *d) {
    rdpq_attach(d, NULL);
    game_start_draw(menu_now);
    rdpq_detach_show();
    frame_rate_end_frame();     /* up on the screen at once */
    mixer_try_play();   /* only the thunk can be playing: it is in memory, everything else was stopped */
}

bool game_start_load_begin (void) {
    if (stage != OPENING) {
        return false;
    }
    /* From here the cart's copy of the menu file is overwritten: no more
       sound (streamed from it), nothing more read from rom:/. */
    for (int channel = 0; channel <= SOUND_BGM_CHANNEL + 1; channel++) {
        mixer_ch_stop(channel);
    }
    stage = LOADING;
    load_began = last_frame = get_ticks_ms();
    load_progress = 0.0f;
    return true;
}

void game_start_progress (float progress) {
    if (stage != LOADING) {
        return;
    }
    load_progress = progress;
    surface_t *d = display_try_get();
    if (d) {
        frame(d);
    }
}

void game_start_finish (void) {
    if (stage != LOADING) {
        return;
    }
    load_progress = 1.0f;
    load_done = true;
    debugf("game start: loaded in %lu ms\n", (unsigned long) (get_ticks_ms() - load_began));
    for (;;) {
        bool done = finished();
        frame(display_get());
        if (done) {
            break;      /* that last frame was black */
        }
    }
}

void game_start_end (void) {
    if (stage == IDLE) {
        return;
    }
    bool failed = !load_done;
    stage = IDLE;
    free_all();
    game_info_ui_hide_cover(false);
    if (failed) {
        volume_apply();     /* the music's level back (loading failed; the error screen follows) */
    }
}
