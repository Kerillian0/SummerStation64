#include <libdragon.h>

#include "debug_stats.h"

#define REPORT_EVERY_US     (2000000)

static uint64_t last_frame_us = 0;
static uint64_t last_report_us = 0;
static uint64_t frame_total_us = 0;
static uint64_t frame_worst_us = 0;
static int frames = 0;
static int reported_mode = -1;

void debug_stats_frame (menu_t *menu) {
    uint64_t now = get_ticks_us();

    /* Start counting afresh on each screen, so one screen's numbers don't mix into the next. */
    if (last_frame_us == 0 || (int) menu->mode != reported_mode) {
        reported_mode = (int) menu->mode;
        last_frame_us = now;
        last_report_us = now;
        frame_total_us = 0;
        frame_worst_us = 0;
        frames = 0;
        return;
    }

    uint64_t frame_us = now - last_frame_us;
    last_frame_us = now;
    frame_total_us += frame_us;
    if (frame_us > frame_worst_us) {
        frame_worst_us = frame_us;
    }
    frames++;

    if ((now - last_report_us) < REPORT_EVERY_US) {
        return;
    }

    heap_stats_t heap;
    sys_get_heap_stats(&heap);

    int average_us = (int) (frame_total_us / frames);
    int worst_us = (int) frame_worst_us;

    debugf(
        "stats: screen %d | heap %d KB, used %d KB, free %d KB | frame avg %d.%d ms, worst %d.%d ms\n",
        reported_mode,
        heap.total / 1024, heap.used / 1024, (heap.total - heap.used) / 1024,
        average_us / 1000, (average_us % 1000) / 100,
        worst_us / 1000, (worst_us % 1000) / 100
    );

    last_report_us = now;
    frame_total_us = 0;
    frame_worst_us = 0;
    frames = 0;
}
