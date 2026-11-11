#include <math.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "box3d.h"
#include "carousel_art.h"
#include "cart_model.h"
#include "cover_deal.h"
#include "cover_list.h"
#include "game_index.h"
#include "menu_features.h"
#include "play_stats.h"

#define DEAL_SETTLED_MS (150)   /* it waits for the covers' art: all loaded (carousel_art_quiet()), the last a moment ago */
#define DEAL_WAIT_MS    (1500)  /* but no longer than this (games without art) */
#define DEAL_DELAY_MS   (0)
#define DEAL_STAGGER_MS (170)   /* between one pair of covers and the next, outermost first */
#define DEAL_SLIDE_MS   (500)   /* a side cover's slide */
#define DEAL_DROP_MS    (560)   /* the selected cover's drop */
#define DEAL_FROM_PX    (150)   /* how far out a side cover starts (kept on the screen) */
#define DEAL_DROP_PX    (70)    /* how far above the selected cover starts */
#define DEAL_SMALL      (0.6f)  /* and how small */
#define DEAL_FARTHEST   (3)     /* the row's outermost place */
#define MIDDLE_START_MS (DEAL_DELAY_MS + DEAL_FARTHEST * DEAL_STAGGER_MS)

/* Back from a game: in the middle's turn, its cartridge rises from the
   bottom edge spinning, ends edge-on, and the cover comes round in its place. */
/* One spin, slowing all the way: the cartridge for the first one and a
   quarter turns, then, as it passes edge-on, still turning, the cover takes
   over and carries on round into its place. */
#define BACK_MS         (1300)
#define BACK_SPIN       (540.0f)    /* in all: one and a half turns */
#define BACK_SWAP       (450.0f)    /* edge-on: the cover takes over from here */
#define BACK_PITCH      (8.0f)
#define SCREEN_BOTTOM   (480)

static bool began = false;
static bool running = false;
static bool waiting = false;        /* for the covers' art (loading it makes slow frames: the deal would skip) */
static bool back = false;           /* back from a game: its cartridge comes back in the middle */
static uint64_t since = 0;          /* the deal started (or the wait, while waiting) */
static uint64_t art_changed = 0;
static int art_seen = -1;
static uint64_t last_frame = 0;     /* how smoothly it ran, for the log */
static int frames = 0, worst = 0;

static int middle_ms (void) {
    return back ? BACK_MS : DEAL_DROP_MS;
}

static int total_ms (void) {
    return MIDDLE_START_MS + middle_ms();
}

/* The game just played, if it is on this row: selected (on the Games list),
   and its cartridge loaded. */
static void back_from_game (menu_t *menu) {
    const char *rom;
    uint32_t length, count, seconds;
    if (!play_stats_settled(&rom, &length, &count, &seconds)) {
        return;
    }
    const cover_list_t *covers = cover_list_current(menu);
    int found = -1;
    if (covers->list == menu->browser.list) {
        for (int i = 0; i < covers->entries && found < 0; i++) {
            path_t *path = covers->path(menu, i);
            if (strcmp(path_get(path), rom) == 0) {
                found = i;
            }
            path_free(path);
        }
        if (found >= 0) {
            menu->browser.selected = found;     /* the row opens on it */
            menu->browser.entry = &menu->browser.list[found];
        }
    } else if (covers->selected >= 0 && covers->selected < covers->entries) {
        path_t *path = covers->path(menu, covers->selected);     /* Recent, Favorites: only if it is the one selected */
        if (strcmp(path_get(path), rom) == 0) {
            found = covers->selected;
        }
        path_free(path);
    }
    if (found < 0 || covers->list[found].type != ENTRY_TYPE_ROM) {
        debugf("cover deal: back from %s, not on this row\n", rom);
        return;
    }
    const game_index_entry_t *known = game_index_find(rom, 0);
    char code[5] = "";
    if (known) {
        memcpy(code, known->code, 4);
    }
    cart_model_load(code[0] ? code : NULL);
    back = cart_model_ready();
    debugf("cover deal: back from %s, cartridge %s\n", rom, cart_model_own() ? "its own" : (back ? "plain" : "none"));
}

void cover_deal_start (menu_t *menu) {
    if (began) {
        return;
    }
    began = true;
    running = waiting = features_enabled(FEATURE_CAROUSEL_ANIMATION);
    since = art_changed = get_ticks_ms();
    if (running) {
        back_from_game(menu);
    }
}

static void finish (void) {
    running = waiting = false;
    if (back) {
        back = false;
        cart_model_free();
    }
}

void cover_deal_frame (bool middle_art, int with_art, bool faded_in) {
    uint64_t now = get_ticks_ms();
    if (!running) {
        return;
    }
    if (animations_last_press_ms() > since) {
        finish();   /* a press finishes it */
        return;
    }
    if (waiting) {
        if (with_art != art_seen) {
            art_seen = with_art;
            art_changed = now;
        }
        bool art_in = (middle_art && carousel_art_quiet() && now - art_changed >= DEAL_SETTLED_MS) || now - since >= DEAL_WAIT_MS;
        if (art_in && faded_in) {
            debugf("cover deal: starts after %d ms waiting for art\n", (int) (now - since));
            waiting = false;
            since = last_frame = now;
            frames = worst = 0;
        }
        return;
    }
    int gap = (int) (now - last_frame);
    last_frame = now;
    frames++;
    worst = (gap > worst) ? gap : worst;
    if (now - since >= (uint64_t) total_ms()) {
        debugf("cover deal: %d frames in %d ms, slowest %d ms\n", frames, (int) (now - since), worst);
        finish();
    }
}

bool cover_deal_running (void) {
    return running;
}

bool cover_deal_ahead (void) {
    return began ? running : features_enabled(FEATURE_CAROUSEL_ANIMATION);
}

static float progress (int offset) {
    int d = (offset < 0) ? -offset : offset;
    int start = DEAL_DELAY_MS + (DEAL_FARTHEST - ((d > DEAL_FARTHEST) ? DEAL_FARTHEST : d)) * DEAL_STAGGER_MS;
    if (d == 0) {
        start = MIDDLE_START_MS;
    }
    return (float) ((int) (get_ticks_ms() - since) - start) / ((d == 0) ? middle_ms() : DEAL_SLIDE_MS);
}

bool cover_deal_place (int offset, int *dx, int *dy, float *scale, float *alpha) {
    *dx = 0;
    *dy = 0;
    *scale = 1.0f;
    *alpha = 1.0f;
    if (!running) {
        return true;
    }
    if (waiting) {
        return false;   /* not dealt yet: the row is empty while its art loads */
    }
    float u = progress(offset);
    if (u < 0.0f) {
        return false;
    }
    if (u >= 1.0f || (offset == 0 && back)) {
        return true;    /* back from a game: the middle is the cartridge's (cover_deal_back()) */
    }
    float e = animations_ease_back(u);     /* a little past its place, and back */
    if (offset == 0) {
        *dy = (int) (-DEAL_DROP_PX * (1.0f - e));
        *scale = DEAL_SMALL + (1.0f - DEAL_SMALL) * e;
    } else {
        *dx = (int) ((offset < 0 ? -DEAL_FROM_PX : DEAL_FROM_PX) * (1.0f - e));
        *alpha = (u * 2.5f > 1.0f) ? 1.0f : u * 2.5f;
    }
    return true;
}

bool cover_deal_back (int x0, int y0, int w, int h, float *turn) {
    *turn = 0.0f;
    if (!running || waiting || !back) {
        return false;
    }
    float t = (float) ((int) (get_ticks_ms() - since) - MIDDLE_START_MS);
    if (t < 0.0f) {
        return false;
    }
    float u = t / BACK_MS;
    if (u >= 1.0f) {
        return false;
    }
    float angle = BACK_SPIN * animations_ease_out(u);
    if (angle < BACK_SWAP) {
        /* The cartridge, up from below the bottom edge: the rise slows to a stop
           just as it passes edge-on (the spin's ease-out reaches BACK_SWAP at swap_u). */
        float swap_u = 1.0f - cbrtf(1.0f - BACK_SWAP / BACK_SPIN);
        float rise = animations_ease_out((u < swap_u) ? u / swap_u : 1.0f);
        int below = (int) ((SCREEN_BOTTOM - y0) * (1.0f - rise));
        box3d_pose_t pose = { .amount = 1.0f, .pitch = BACK_PITCH };
        cart_model_draw(&pose, x0, y0 + below, w, h, angle, 255, 255);
        return true;
    }
    *turn = angle - BACK_SPIN;     /* the cover: from edge-on (-90) on round to its place (0) */
    return false;
}

bool cover_deal_hides_ring (void) {
    return running && (waiting || progress(0) < 1.0f);
}
