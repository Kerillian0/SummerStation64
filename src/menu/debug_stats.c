#include <libdragon.h>

#include "debug_stats.h"

#define REPORT_EVERY_US     (2000000)

static uint64_t last_frame_us = 0;
static uint64_t last_report_us = 0;
static uint64_t frame_total_us = 0;
static uint64_t frame_worst_us = 0;
static int frames = 0;
static int reported_mode = -1;

/* "Work" is the part of a frame the processor spends on it: from getting a
   screen buffer to having drawn the screen and done its loading. The rest of
   the frame is waiting for the graphics chip and for the TV's next refresh. */
static uint64_t work_began_us = 0;
static uint64_t work_total_us = 0;
static uint64_t work_worst_us = 0;
static uint64_t work_before_us = 0;

/* A frame longer than this missed its turn (one frame is 33.4 ms). */
#define SLOW_FRAME_US       (45000)

void debug_stats_begin (void) {
    work_began_us = get_ticks_us();
}

void debug_stats_frame (menu_t *menu) {
    uint64_t now = get_ticks_us();

    /* Start counting afresh on each screen, so one screen's numbers don't mix into the next. */
    if (last_frame_us == 0 || (int) menu->mode != reported_mode) {
        reported_mode = (int) menu->mode;
        last_frame_us = now;
        last_report_us = now;
        frame_total_us = 0;
        frame_worst_us = 0;
        work_total_us = 0;
        work_worst_us = 0;
        work_before_us = 0;
        frames = 0;
        return;
    }

    uint64_t work_us = now - work_began_us;
    work_total_us += work_us;
    if (work_us > work_worst_us) {
        work_worst_us = work_us;
    }

    uint64_t frame_us = now - last_frame_us;
    last_frame_us = now;
    frame_total_us += frame_us;
    if (frame_us > frame_worst_us) {
        frame_worst_us = frame_us;
    }
    frames++;

    if (frame_us > SLOW_FRAME_US) {
        debugf("slow frame: %d ms (work %d ms, the frame before %d ms)\n",
            (int) (frame_us / 1000), (int) (work_us / 1000), (int) (work_before_us / 1000));
    }
    work_before_us = work_us;

    if ((now - last_report_us) < REPORT_EVERY_US) {
        return;
    }

    heap_stats_t heap;
    sys_get_heap_stats(&heap);

    int average_us = (int) (frame_total_us / frames);
    int worst_us = (int) frame_worst_us;
    int work_average_us = (int) (work_total_us / frames);
    int work_most_us = (int) work_worst_us;

    debugf(
        "stats: screen %d | heap %d KB, used %d KB, free %d KB | frame avg %d.%d ms, worst %d.%d ms | work avg %d.%d ms, worst %d.%d ms\n",
        reported_mode,
        heap.total / 1024, heap.used / 1024, (heap.total - heap.used) / 1024,
        average_us / 1000, (average_us % 1000) / 100,
        worst_us / 1000, (worst_us % 1000) / 100,
        work_average_us / 1000, (work_average_us % 1000) / 100,
        work_most_us / 1000, (work_most_us % 1000) / 100
    );

    last_report_us = now;
    frame_total_us = 0;
    frame_worst_us = 0;
    work_total_us = 0;
    work_worst_us = 0;
    frames = 0;
}
