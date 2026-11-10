/* Test ROM for the Game info preview clips: plays rom:/preview.h264 with the
   menu's own player (src/menu/video_preview.c) in the cover's place, at the
   menu's resolution and 30-a-second cap, while pretending to be a menu that
   spends 0, 12 or 20 ms of CPU on each frame (25 seconds of each, round and
   round). How well the clip and the "menu" keep up is shown on screen and
   logged every 5 seconds (ISViewer: ares prints it). */

#include <libdragon.h>

#include "video_preview.h"

#define CLIP        "rom:/preview.h264"
#define COVER_X     (424)
#define COVER_Y     (44)
#define COVER_W     (176)
#define COVER_H     (126)
#define PHASE_MS    (25000)
#define REPORT_MS   (5000)

static const int load_ms[] = { 0, 12, 20 };
#define PHASES ((int) (sizeof(load_ms) / sizeof(load_ms[0])))

static void fill (int x0, int y0, int x1, int y1, color_t c) {
    rdpq_set_mode_fill(c);
    rdpq_fill_rectangle(x0, y0, x1, y1);
}

int main (void) {
    debug_init_isviewer();
    debug_init_usblog();
    dfs_init(DFS_DEFAULT_LOCATION);

    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    display_set_fps_limit(display_get_refresh_rate() / 2.0f);     /* as the menu does */
    rdpq_init();
    rdpq_text_register_font(1, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR));

    debugf("video preview test: %s, Expansion Pak %s\n", CLIP, is_memory_expanded() ? "yes" : "no");

    uint64_t start = get_ticks_ms();
    uint64_t report_at = start + REPORT_MS;
    int phase = 0;

    /* Since the last report */
    uint32_t menu_frames = 0, menu_slow = 0;
    uint32_t upd_max = 0;
    uint64_t last_frame_ms = start;
    uint32_t shown_at_report = 0, late_at_report = 0, idle_at_report = 0;
    float menu_fps = 0, clip_fps = 0;

    while (1) {
        surface_t *disp = display_try_get();
        if (!disp) {
            video_preview_poll();       /* the menu's idle time */
            continue;
        }
        uint64_t now = get_ticks_ms();
        phase = (int) (((now - start) / PHASE_MS) % PHASES);
        if (now - last_frame_ms > 40) {
            menu_slow++;                /* a frame that missed its 33 ms */
        }
        last_frame_ms = now;

        /* The rest of the menu's work this frame. */
        uint64_t busy = get_ticks_us();
        while (get_ticks_us() - busy < (uint64_t) load_ms[phase] * 1000) {
        }

        video_preview_update(CLIP);
        const video_preview_stats_t *s = video_preview_stats();
        upd_max = (s->update_us > upd_max) ? s->update_us : upd_max;

        rdpq_attach(disp, NULL);
        fill(0, 0, 640, 480, RGBA32(0x18, 0x1C, 0x2C, 0xFF));
        fill(40, 182, 600, 420, RGBA32(0x24, 0x2A, 0x40, 0xFF));    /* the description box */
        int w = COVER_W, h = COVER_H;
        video_preview_fit(COVER_W, COVER_H, &w, &h);
        int x = COVER_X + (COVER_W - w) / 2, y = COVER_Y + (COVER_H - h) / 2;
        fill(x - 3, y - 3, x + w + 3, y + h + 3, RGBA32(0xE0, 0x90, 0x30, 0xFF));
        if (!video_preview_draw(COVER_X, COVER_Y, COVER_W, COVER_H)) {
            fill(x, y, x + w, y + h, RGBA32(0x50, 0x50, 0x60, 0xFF));  /* "the cover", until the clip starts */
        }

        rdpq_text_printf(NULL, 1, 48, 60, "Preview clip test");
        rdpq_text_printf(NULL, 1, 48, 80, "pretend menu work: %d ms a frame", load_ms[phase]);
        rdpq_text_printf(NULL, 1, 48, 200,
            "clip %dx%d at %.0f fps\n"
            "menu: %.1f fps, %lu slow frames\n"
            "clip: %.1f fps shown, %lu late, %lu loops\n"
            "update: %lu us (most %lu), loop %lu us\n"
            "idle decode slices: %lu",
            s->width, s->height, s->fps,
            menu_fps, menu_slow,
            clip_fps, s->late, s->loops,
            s->update_us, upd_max, s->loop_us_max,
            s->idle_slices);
        rdpq_detach_show();
        rspq_wait();    /* as the menu's frame_rate_end_frame() does */
        menu_frames++;

        if (now >= report_at) {
            float secs = (now - report_at + REPORT_MS) / 1000.0f;
            menu_fps = menu_frames / secs;
            clip_fps = (s->frames_shown - shown_at_report) / secs;
            debugf("t=%3lus work=%2dms | menu %.1f fps, %lu slow | clip %.1f fps (of %.0f), late %lu, loops %lu | "
                "update max %lu us, next max %lu us, loop-at-once %lu us | idle slices %lu\n",
                (uint32_t) ((now - start) / 1000), load_ms[phase], menu_fps, menu_slow,
                clip_fps, s->fps, s->late - late_at_report, s->loops, upd_max, s->next_us_max, s->loop_us_max,
                s->idle_slices - idle_at_report);
            shown_at_report = s->frames_shown;
            late_at_report = s->late;
            idle_at_report = s->idle_slices;
            menu_frames = 0;
            menu_slow = 0;
            upd_max = 0;
            report_at = now + REPORT_MS;
        }
    }
}
