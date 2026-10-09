#include <libdragon.h>

#include "frame_rate.h"
#include "menu_options.h"

const char *frame_rate_name (int choice) {
    return (choice == FRAME_RATE_FULL) ? "60" : "30";
}

static uint64_t last_wait_us = 0;

void frame_rate_end_frame (void) {
    /* A finished frame is put on screen by a call the graphics code makes
       "when it next gets the chance", and with the menu just idling between
       frames that chance came whenever the sound code happened to need the
       chip: sometimes too late for the refresh, so a frame that was ready in
       time was shown 33 ms late. Waiting here makes it happen the moment the
       chip is done (2-3 ms, time the menu would have spent idling anyway). */
    uint64_t began = get_ticks_us();
    rspq_wait();
    last_wait_us = get_ticks_us() - began;
}

uint64_t frame_rate_last_wait_us (void) {
    return last_wait_us;
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
