#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "carousel_art.h"
#include "cover_clip.h"
#include "cover_list.h"
#include "games_filter.h"
#include "hack_games.h"
#include "menu_options.h"
#include "rom_hacks.h"
#include "video_preview.h"

#define LOOK_AFTER_MS   (500)   /* rested this long: look for its clip; video_preview.c waits 1.5 s more, so it starts at 2 s */

static const void *list_seen = NULL;
static int index_seen = -2;
static uint64_t rest_since = 0;
static bool looked = false;
static bool found = false;
static bool playing = false;     /* video_preview_update() has been called for it since the last rest began */
static char clip_path[256];

const char *cover_clip_name (int choice) {
    static const char *names[2] = { "Off", "On" };
    return (choice >= 0 && choice < 2) ? names[choice] : "";
}

static bool wanted (void) {
    return options_get(OPTION_COVER_CLIPS) != 0;
}

/* Any button held down (B to turn the box, a direction held): not resting. */
static bool buttons_held (void) {
    for (int port = 0; port < 4; port++) {
        if (joypad_is_connected(port) && joypad_get_buttons(port).raw) {
            return true;
        }
    }
    return false;
}

static bool handing_over = false;   /* A pressed while the clip plays: it goes on into Game info */

void cover_clip_reset (void) {
    if (playing && !handing_over) {
        video_preview_stop();
        carousel_art_clip_free();
    }
    playing = false;
    handing_over = false;
    looked = false;
    found = false;
    index_seen = -2;
    list_seen = NULL;
}

/* A held on any controller: the game's info opens (or it starts) on letting go. */
static bool a_held (void) {
    for (int port = 0; port < 4; port++) {
        if (joypad_is_connected(port) && joypad_get_buttons(port).a) {
            return true;
        }
    }
    return false;
}

/* The code a game's clip goes by: a hack's own (hacks.txt), else the game's,
   but none for a hack that shares its original's (as on Game info). */
static bool clip_code (menu_t *menu, int index, char code[5]) {
    const cover_list_t *covers = cover_list_current(menu);
    if (covers->list[index].type != ENTRY_TYPE_ROM) {
        return false;   /* disks and emulator games have no clips */
    }
    path_t *path = covers->path(menu, index);
    const char *full = path_get(path);
    bool ok = false;
    if (hack_games_code(full, covers->list[index].size, code)) {
        code[4] = '\0';
        ok = true;
    } else {
        const char *name = strrchr(full, '/');
        name = name ? name + 1 : full;
        const char *on_card = strstr(full, ":/");
        on_card = on_card ? on_card + 2 : full;
        bool hack = games_filter_is_hack_path(menu, full)
            || games_filter_decide(on_card, false, rom_hacks_name_says(name)) == GAME_KIND_HACK;
        ok = !hack && carousel_art_code(menu, index, code);
    }
    path_free(path);
    return ok;
}

void cover_clip_after_draw (menu_t *menu) {
    uint64_t now = get_ticks_ms();
    const cover_list_t *covers = cover_list_current(menu);
    int selected = covers->selected;

    /* A on a playing clip: Game info is about to open, and the clip goes
       on there (video_preview_carry_on()); it isn't stopped here. */
    if (playing && selected == index_seen && a_held()) {
        handing_over = true;
        video_preview_carry_on();
        return;
    }
    handing_over = false;

    /* Another game, a press, or a button held: the rest starts again and a
       clip that was showing stops (the cover is back at once). */
    bool moved = (selected != index_seen || (const void *) covers->list != list_seen);
    uint64_t pressed = animations_last_press_ms();
    if (moved || buttons_held() || pressed > rest_since) {
        if (playing) {
            video_preview_stop();
            carousel_art_clip_free();
            playing = false;
        }
        rest_since = (moved || buttons_held()) ? now : pressed;
        if (moved) {
            index_seen = selected;
            list_seen = covers->list;
            looked = false;
            found = false;
        }
    }

    if (!looked && wanted() && selected >= 0 && selected < covers->entries && now - rest_since >= LOOK_AFTER_MS) {
        looked = true;
        char code[5];
        found = clip_code(menu, selected, code)
            && video_preview_find(menu->storage_prefix, code, clip_path, sizeof(clip_path));
    }
}

float cover_clip_frame (menu_t *menu, int index) {
    if (!found || !wanted() || index != index_seen || get_ticks_ms() - rest_since < LOOK_AFTER_MS) {
        return 1.0f;
    }
    video_preview_update(clip_path);
    playing = true;
    int w, h;
    if (!video_preview_fit(64, 64, &w, &h)) {
        return 1.0f;    /* still loading, or the cover's moment alone */
    }
    return 1.0f - video_preview_fade();
}

float cover_clip_draw (menu_t *menu, int index, int x0, int y0, int w, int h, int *shown_w, int *shown_h) {
    float cover_left = cover_clip_frame(menu, index);
    if (cover_left >= 1.0f || !video_preview_fit(w, h, shown_w, shown_h)) {
        return 1.0f;
    }
    video_preview_draw(x0, y0, w, h);
    return cover_left;
}
