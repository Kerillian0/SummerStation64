#include <libdragon.h>

#include "safe_mode.h"

/* A one-off, blocking controller read. libdragon provides it (its crash
   screen uses it too) but doesn't list it in the public joypad.h. */
joypad_inputs_t joypad_read_n64_inputs (joypad_port_t port);

static bool active = false;

void safe_mode_detect (void) {
    /* Read the controllers directly: the regular polling isn't running this early. */
    JOYPAD_PORT_FOREACH (port) {
        if (joypad_read_n64_inputs(port).btn.z) {
            active = true;
        }
    }
    debugf("safe mode: %s\n", active ? "on" : "off");
}

bool safe_mode_active (void) {
    return active;
}
