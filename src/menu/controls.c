#include <libdragon.h>

#include "controls.h"
#include "menu_features.h"

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
}
