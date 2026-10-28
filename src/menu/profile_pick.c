#include <libdragon.h>

#include "fonts.h"
#include "games_ui.h"
#include "intro.h"
#include "profile_pick.h"
#include "profiles.h"
#include "sound.h"
#include "theme.h"
#include "title_font.h"
#include "ui_components.h"
#include "ui_components/constants.h"

#define FADE_IN_MS      (400)
#define TITLE_Y         (104)
#define TITLE_HEIGHT    (40)
#define ROW_WIDTH       (320)
#define ROW_HEIGHT      (48)
#define ROW_STEP        (60)
#define ROWS_Y          (176)
#define RING            (4)
#define DOT             (20)

static bool showing = false;
static int selected = 0;
static menu_mode_t after;
static uint64_t shown_at;
static bool started;

void profile_pick_begin (menu_t *menu) {
    if (!intro_power_on() || profiles_count() < 2) {
        return;
    }
    after = intro_hold(menu);
    selected = profiles_current();
    showing = true;
    started = false;
    debugf("profiles: asking who is playing (%d profiles, last %s)\n", profiles_count(), profiles_name(selected));
}

static void process (menu_t *menu) {
    int count = profiles_count();
    if (menu->actions.go_up || menu->actions.go_left) {
        selected = (selected + count - 1) % count;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down || menu->actions.go_right) {
        selected = (selected + 1) % count;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        sound_play_effect(SFX_ENTER);
        if (selected != profiles_current()) {
            profiles_switch(selected);
        }
        debugf("profiles: %s is playing\n", profiles_name(selected));
        showing = false;
        menu->next_mode = after;
    }
}

static void draw (surface_t *display) {
    const theme_t *t = theme_get();
    int x0 = DISPLAY_CENTER_X - (ROW_WIDTH / 2);

    rdpq_attach(display, NULL);
    ui_components_background_draw();

    const char *title = "Who's playing?";
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = DISPLAY_WIDTH,
        .height = TITLE_HEIGHT,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .style_id = STL_DEFAULT,
    }, title_font_pick(title, DISPLAY_WIDTH), 0, TITLE_Y, "%s", title);

    for (int i = 0; i < profiles_count(); i++) {
        int y = ROWS_Y + (i * ROW_STEP);
        if (i == selected) {
            games_ui_round_fill(x0 - RING, y - RING, x0 + ROW_WIDTH + RING, y + ROW_HEIGHT + RING, t->accent);
        }
        games_ui_round_fill(x0, y, x0 + ROW_WIDTH, y + ROW_HEIGHT, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xFF));
        games_ui_round_fill(x0 + 16, y + ((ROW_HEIGHT - DOT) / 2), x0 + 16 + DOT, y + ((ROW_HEIGHT + DOT) / 2), profiles_color(i));
        int name_x = x0 + 16 + DOT + 14;
        int name_width = x0 + ROW_WIDTH - 12 - name_x;
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = name_width,
            .height = ROW_HEIGHT,
            .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES,
            .style_id = STL_DEFAULT,
        }, title_font_pick(profiles_name(i), name_width), name_x, y, "%s", profiles_name(i));
    }

    games_ui_hints_backdrop_draw();
    int hint_x = DISPLAY_CENTER_X - (games_ui_hint_width("A", "Play") / 2);
    games_ui_hint_draw(hint_x, 1, "A", "Play");

    /* Up from black after the intro. */
    int t_ms = (int) (get_ticks_ms() - shown_at);
    if (t_ms < FADE_IN_MS) {
        int dark = 255 - ((t_ms * 255) / FADE_IN_MS);
        rdpq_mode_push();
            rdpq_set_mode_standard();
            rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, dark));
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        rdpq_mode_pop();
    }

    rdpq_detach_show();
}

bool profile_pick_display (menu_t *menu, surface_t *display) {
    if (!showing) {
        return false;
    }
    if (!started) {
        started = true;
        shown_at = get_ticks_ms();
    } else {
        process(menu);
    }
    intro_fade_wait();  /* the first tab's fade-in (and the music) start once someone is chosen */
    draw(display);
    return true;
}
