#include <libdragon.h>

#include "animations.h"
#include "cover_deal.h"
#include "menu_features.h"

#define DEAL_SETTLED_MS (350)   /* it waits for the covers' art: no new cover for this long, the middle one in */
#define DEAL_WAIT_MS    (1500)  /* but no longer than this (games without art) */
#define DEAL_DELAY_MS   (0)
#define DEAL_STAGGER_MS (170)   /* between one pair of covers and the next, outermost first */
#define DEAL_SLIDE_MS   (500)   /* a side cover's slide */
#define DEAL_DROP_MS    (560)   /* the selected cover's drop */
#define DEAL_FROM_PX    (150)   /* how far out a side cover starts (kept on the screen) */
#define DEAL_DROP_PX    (70)    /* how far above the selected cover starts */
#define DEAL_SMALL      (0.6f)  /* and how small */
#define DEAL_FARTHEST   (3)     /* the row's outermost place */
#define DEAL_TOTAL_MS   (DEAL_DELAY_MS + DEAL_FARTHEST * DEAL_STAGGER_MS + DEAL_DROP_MS)

static bool began = false;
static bool running = false;
static bool waiting = false;        /* for the covers' art (loading it makes slow frames: the deal would skip) */
static uint64_t since = 0;          /* the deal started (or the wait, while waiting) */
static uint64_t art_changed = 0;
static int art_seen = -1;
static uint64_t last_frame = 0;     /* how smoothly it ran, for the log */
static int frames = 0, worst = 0;

void cover_deal_frame (bool middle_art, int with_art, bool faded_in) {
    uint64_t now = get_ticks_ms();
    if (!began) {
        began = true;
        running = waiting = features_enabled(FEATURE_CAROUSEL_ANIMATION);
        since = art_changed = now;
        art_seen = with_art;
        return;
    }
    if (running && animations_last_press_ms() > since) {
        running = waiting = false;  /* a press finishes it */
        return;
    }
    if (waiting) {
        if (with_art != art_seen) {
            art_seen = with_art;
            art_changed = now;
        }
        bool art_in = (middle_art && now - art_changed >= DEAL_SETTLED_MS) || now - since >= DEAL_WAIT_MS;
        if (art_in && faded_in) {
            debugf("cover deal: starts after %d ms waiting for art\n", (int) (now - since));
            waiting = false;
            since = last_frame = now;
            frames = worst = 0;
        }
        return;
    }
    if (running) {
        int gap = (int) (now - last_frame);
        last_frame = now;
        frames++;
        worst = (gap > worst) ? gap : worst;
    }
    if (running && now - since >= DEAL_TOTAL_MS) {
        running = false;
        debugf("cover deal: %d frames in %d ms, slowest %d ms\n", frames, (int) (now - since), worst);
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
        start = DEAL_DELAY_MS + DEAL_FARTHEST * DEAL_STAGGER_MS;
    }
    return (float) ((int) (get_ticks_ms() - since) - start) / ((d == 0) ? DEAL_DROP_MS : DEAL_SLIDE_MS);
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
    if (u >= 1.0f) {
        return true;
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

bool cover_deal_hides_ring (void) {
    return running && (waiting || progress(0) < 1.0f);
}
