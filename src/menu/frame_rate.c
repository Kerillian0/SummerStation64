#include <libdragon.h>

#include "frame_rate.h"
#include "menu_options.h"

const char *frame_rate_name (int choice) {
    return (choice == FRAME_RATE_FULL) ? "60" : "30";
}

void frame_rate_apply (void) {
    /* 60 is as fast as the display refreshes, so in practice "no cap". On a
       PAL console that comes out as 50. */
    display_set_fps_limit((options_get(OPTION_FRAME_RATE) == FRAME_RATE_FULL) ? 60.0f : 30.0f);
}
