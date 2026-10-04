#include <libdragon.h>

#include "controls.h"
#include "menu_features.h"

#define HOLD_TIME_MS    (500)

static bool launch_request = false;
static bool a_pending = false;
static uint64_t a_pressed_at;

static bool a_is_held (void) {
    JOYPAD_PORT_FOREACH (i) {
        // Current state; "held" would be false on the frame A goes down.
        if (joypad_get_buttons(i).a) {
            return true;
        }
    }
    return false;
}

/* Tells a tap of A from a hold, and turns either into a normal "enter". */
static void controls_tap_or_hold (menu_t *menu) {
    bool quick = features_enabled(FEATURE_QUICK_LAUNCH);
    bool hybrid = features_enabled(FEATURE_HOLD_LAUNCH);

    launch_request = false;

    if (!quick && !hybrid) {
        a_pending = false;
        return;
    }

    if (menu->actions.enter) {
        a_pending = true;
        a_pressed_at = get_ticks_ms();
    }

    if (!a_pending) {
        return;
    }

    /* Nothing else happens until we know which kind of press it is. */
    menu->actions = (typeof(menu->actions)) {0};

    bool held = a_is_held();
    bool long_press = held && ((get_ticks_ms() - a_pressed_at) >= HOLD_TIME_MS);

    if (long_press || !held) {
        a_pending = false;
        menu->actions.enter = true;
        launch_request = quick ? !long_press : long_press;
    }
}

bool controls_consume_launch_request (void) {
    bool request = launch_request;
    launch_request = false;
    return request;
}

void controls_remap_tabs (menu_t *menu, bool horizontal) {
    joypad_buttons_t pressed = {0};

    JOYPAD_PORT_FOREACH (i) {
        pressed = joypad_get_buttons_pressed(i);
        if (pressed.raw) {
            break;
        }
    }

    if (horizontal) {
        bool updown = features_enabled(FEATURE_UPDOWN_SCROLL);
        bool previous = menu->actions.go_left || (updown && menu->actions.go_up);
        bool next = menu->actions.go_right || (updown && menu->actions.go_down);
        menu->actions.go_up = previous;
        menu->actions.go_down = next;
    }

    menu->actions.go_left = pressed.l;
    menu->actions.go_right = pressed.r;
    menu->actions.options = pressed.z;
    menu->actions.lz_context = false;

    controls_tap_or_hold(menu);
}
