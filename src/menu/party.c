/**
 * @file party.c
 * @brief Party mode: how many controllers are plugged in.
 */

#include <libdragon.h>

#include "menu_options.h"
#include "party.h"

bool party_port_in_use (int port) {
    int test = options_get(OPTION_PARTY_TEST);
    if (test > 0) {
        return port < test;
    }
    return port >= 0 && port < JOYPAD_PORT_COUNT && joypad_is_connected((joypad_port_t) port);
}

int party_players (void) {
    int n = 0;
    for (int port = 0; port < JOYPAD_PORT_COUNT; port++) {
        n += party_port_in_use(port) ? 1 : 0;
    }
    return n;
}
