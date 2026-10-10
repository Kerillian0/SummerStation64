#include <libdragon.h>

#include "debug_stats.h"
#include "fonts.h"
#include "frame_rate.h"
#include "menu_options.h"

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

/* "Chip" runs from the start of a frame's work until the graphics chip has
   drawn it (frame_rate_end_frame() waits for that); over 33 ms means the
   frame missed its refresh. */
static uint64_t chip_total_us = 0;
static uint64_t chip_worst_us = 0;

/* A frame longer than this missed its turn (one frame is 33.4 ms). */
#define SLOW_FRAME_US       (45000)

/* The on-screen overlay (option `perf_overlay`): the same figures over the
   last half second, so they can be read on the TV without the debug log. */
#define OVERLAY_EVERY_US    (500000)
static uint64_t ov_began_us = 0;
static uint64_t ov_frame_total = 0, ov_frame_worst = 0, ov_chip_total = 0, ov_chip_worst = 0;
static int ov_frames = 0;
static char ov_text[96] = "";

static const char *overlay_names[2] = { "Off", "On" };

const char *debug_stats_overlay_name (int choice) {
    return (choice >= 0 && choice < 2) ? overlay_names[choice] : "";
}

static void overlay_count (uint64_t now, uint64_t frame_us, uint64_t chip_us) {
    if (!options_get(OPTION_PERF_OVERLAY)) {
        return;
    }
    ov_frame_total += frame_us;
    ov_chip_total += chip_us;
    if (frame_us > ov_frame_worst) ov_frame_worst = frame_us;
    if (chip_us > ov_chip_worst) ov_chip_worst = chip_us;
    ov_frames++;
    if (ov_began_us == 0) {
        ov_began_us = now;
    }
    if ((now - ov_began_us) < OVERLAY_EVERY_US || ov_frames == 0) {
        return;
    }
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    int frame_avg = (int) (ov_frame_total / ov_frames);
    int chip_avg = (int) (ov_chip_total / ov_frames);
    snprintf(ov_text, sizeof(ov_text), "%d.%d ms (worst %d)  chip %d.%d (worst %d)  free %d KB",
        frame_avg / 1000, (frame_avg % 1000) / 100, (int) (ov_frame_worst / 1000),
        chip_avg / 1000, (chip_avg % 1000) / 100, (int) (ov_chip_worst / 1000),
        (heap.total - heap.used) / 1024);
    ov_began_us = now;
    ov_frame_total = ov_frame_worst = ov_chip_total = ov_chip_worst = 0;
    ov_frames = 0;
}

void debug_stats_overlay_draw (void) {
    if (!options_get(OPTION_PERF_OVERLAY) || ov_text[0] == '\0') {
        return;
    }
    /* A dark strip at the very bottom, below the button hints. */
    const int x0 = 64, x1 = 576, y0 = 434, y1 = 456;
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, 0xC0));
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = x1 - x0,
        .height = y1 - y0,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = STL_WHITE,
    }, FNT_DEFAULT, x0, y0, "%s", ov_text);
}

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
        chip_total_us = 0;
        chip_worst_us = 0;
        frames = 0;
        return;
    }

    uint64_t chip_us = now - work_began_us;
    uint64_t work_us = chip_us - frame_rate_last_wait_us();
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

    chip_total_us += chip_us;
    if (chip_us > chip_worst_us) {
        chip_worst_us = chip_us;
    }

    overlay_count(now, frame_us, chip_us);

    if (frame_us > SLOW_FRAME_US || chip_us > 33000) {
        debugf("slow frame: %d ms (work %d ms, chip %d ms, the frame before %d ms)\n",
            (int) (frame_us / 1000), (int) (work_us / 1000), (int) (chip_us / 1000), (int) (work_before_us / 1000));
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
    int chip_average_us = (int) (chip_total_us / frames);
    int chip_most_us = (int) chip_worst_us;

    debugf(
        "stats: screen %d | heap %d KB, used %d KB, free %d KB | frame avg %d.%d ms, worst %d.%d ms | work avg %d.%d ms, worst %d.%d ms | chip avg %d.%d ms, worst %d.%d ms\n",
        reported_mode,
        heap.total / 1024, heap.used / 1024, (heap.total - heap.used) / 1024,
        average_us / 1000, (average_us % 1000) / 100,
        worst_us / 1000, (worst_us % 1000) / 100,
        work_average_us / 1000, (work_average_us % 1000) / 100,
        work_most_us / 1000, (work_most_us % 1000) / 100,
        chip_average_us / 1000, (chip_average_us % 1000) / 100,
        chip_most_us / 1000, (chip_most_us % 1000) / 100
    );

    last_report_us = now;
    frame_total_us = 0;
    frame_worst_us = 0;
    work_total_us = 0;
    work_worst_us = 0;
    chip_total_us = 0;
    chip_worst_us = 0;
    frames = 0;
}
