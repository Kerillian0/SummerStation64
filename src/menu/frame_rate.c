#include <libdragon.h>

#include "frame_rate.h"
#include "menu_options.h"

const char *frame_rate_name (int choice) {
    return (choice == FRAME_RATE_FULL) ? "60" : "30";
}

void frame_rate_apply (void) {
    /* 60 is as fast as the display refreshes, so in practice "no cap". On a
       PAL console that comes out as 50. */
    /* The cap is exactly every second refresh, not "30": an NTSC console
       refreshes 59.83 times a second, and asking for 30 of those made the
       display code change which refreshes it used every six seconds, at the
       price of one 50 ms frame each time. On PAL this comes out as a steady
       25 instead of an uneven 30. */
    float refresh = display_get_refresh_rate();
    display_set_fps_limit((options_get(OPTION_FRAME_RATE) == FRAME_RATE_FULL) ? refresh : (refresh / 2.0f));
}
