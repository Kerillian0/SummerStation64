#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libdragon.h>
#include <system.h>  /* attach_filesystem */

#include "frame_rate.h"
#include "memory_note.h"
#include "video_clip_check.h"
#include "video_preview.h"

#define START_DELAY_MS      (1500)      /* the cover alone first */
#define FADE_MS             (400)
#define LOAD_STEP           (64 * 1024) /* the clip's header is checked once this much is in */
#define LOAD_PIECE          (4 * 1024)  /* read in the idle time between frames, a piece at a time: about 4 ms each */
#define LOAD_MARGIN_US      (3000)      /* a piece is only started if it should end this long before the next frame is due */
#define LOAD_STALL_MS       (300)       /* ... or if none has been read for this long, so loading never stops */
#define MAX_CLIP_BYTES      (3 * 1024 * 1024)
#define BUFFERED_PICS       (4)         /* pictures decoded ahead: about 42 KB each at 192x144 */
#define FRAME_BUDGET_US     (2000)      /* decoding inside a menu frame, only while the idle time falls short */
#define HOLD_FROM_END       (3)         /* the last pictures copied for that, from the counted end */
#define LOOP_FRAMES         (3)         /* menu frames the last picture is held for while the first decodes again */
#define MAX_STEP_MS         (100)       /* the clip's clock never jumps more than this in one frame */
#define AWAY_MS             (250)       /* not drawn for this long: the screen was left or the picture changed */

/* libdragon's decoder stops the console if it can't get memory, so the room
   is checked first. It wants about 16 pictures' worth (the ones decoded
   ahead, its reference pictures, the held one) plus a little for itself:
   about 920 KB at 192x144, measured at 0.9 MB in ares. RESERVE is left for
   the rest of the menu, as elsewhere (carousel_art.c). */
#define DECODER_BYTES(w, h) (256 * 1024 + 16 * ((w) * (h) * 3 / 2))
#define RESERVE_BYTES       (256 * 1024)

typedef enum {
    VP_OFF,         /* nothing wanted */
    VP_LOADING,     /* reading the clip from the card */
    VP_WAITING,     /* in memory, waiting out START_DELAY_MS */
    VP_PLAYING,
    VP_FAILED,      /* not a clip the player can take: show the cover, try nothing until another game */
} vp_state_t;

static vp_state_t state = VP_OFF;
static char want_path[256];
static uint64_t wanted_ms;          /* when this clip was first asked for */
static uint64_t last_update_ms;

/* Loading */
static FILE *load_file = NULL;
static uint8_t *clip_data = NULL;
static int clip_size = 0;
static int clip_loaded = 0;
static int clip_w = 0, clip_h = 0;  /* from its header */

/* Playing */
static bool codec_ready = false;
static video_t *video = NULL;
static video_info_t info;
static bool have_frame = false;
static int frames_shown = 0;        /* since the start of this pass through the clip */
static double clip_seconds = 0.0;   /* the clip's own clock */
static uint64_t fade_since_ms = 0;
static int frame_count = 0;         /* pictures in the clip (counted when loaded), 0 if unknown */
static uint8_t *hold = NULL;        /* a copy of the last picture, for going round smoothly */
static bool hold_ready = false;
static int looping = 0;             /* menu frames left showing the held picture */
static int behind = 0;              /* frames left of decoding inside the frame too */

static video_preview_stats_t stats;

static yuv_blitter_t blitter = { 0 };
static int blit_x = -1, blit_y = -1, blit_w = -1, blit_h = -1;

/* ---------- the clip in memory, as files the decoder can read ----------

   libdragon's H.264 player reads its stream with open()/read(). Rather than
   have it read the SD card while playing (a card read can take longer than
   a frame), it reads the copy in memory through this tiny filesystem.

   Beside the clip it offers the clip's seek table, preview.seek, when the
   card has one (written by libdragon's own videoconv64, see
   scripts/make_preview_clip.py; its format is libdragon's business). With a
   table, the player goes back to the start by just moving its place in the
   stream, and the first picture is then decoded a slice at a time like any
   other; without one, going back decodes that whole picture there and then
   (about 28 ms, almost a frame), so the clip still loops, just less
   smoothly. */

#define MEM_PREFIX  "vclip:/"
#define MEM_NAME    "/preview.h264"
#define SEEK_NAME   "/preview.seek"     /* found by the player itself: the clip's name with .seek */
#define MAX_SEEK_BYTES  (16 * 1024)     /* a few entries; far more than a 30 second clip needs */

static uint8_t *seek_data = NULL;   /* the card's preview.seek, or NULL */
static int seek_size = 0;

typedef struct {
    const uint8_t *data;
    int size;
    int pos;
    bool open;
} mem_file_t;

static mem_file_t mem_files[2];    /* the clip, the seek table */

static mem_file_t *mem_find (const char *name) {
    if (!clip_data) {
        return NULL;
    }
    /* libdragon's open() passes "/preview.h264" but its stat() "//preview.seek":
       the slashes in front don't matter here. */
    while (name[0] == '/' && name[1] == '/') {
        name++;
    }
    if (strcmp(name, MEM_NAME) == 0) {
        mem_files[0].data = clip_data;
        mem_files[0].size = clip_size;
        return &mem_files[0];
    }
    if (strcmp(name, SEEK_NAME) == 0 && seek_data) {
        mem_files[1].data = seek_data;
        mem_files[1].size = seek_size;
        return &mem_files[1];
    }
    return NULL;
}

static void *mem_open (char *name, int flags) {
    (void) flags;
    mem_file_t *f = mem_find(name);
    if (!f || f->open) {
        errno = ENOENT;
        return NULL;
    }
    f->pos = 0;
    f->open = true;
    return f;
}

static int mem_fstat (void *file, struct stat *st) {
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG;
    st->st_size = ((mem_file_t *) file)->size;
    return 0;
}

static int mem_stat (char *name, struct stat *st) {
    mem_file_t *f = mem_find(name);
    if (!f) {
        errno = ENOENT;
        return -1;
    }
    return mem_fstat(f, st);
}

static int mem_lseek (void *file, int offset, int whence) {
    mem_file_t *f = file;
    int to = (whence == SEEK_SET) ? offset : (whence == SEEK_CUR) ? (f->pos + offset) : (f->size + offset);
    if (to < 0 || to > f->size) {
        errno = EINVAL;
        return -1;
    }
    f->pos = to;
    return f->pos;
}

static int mem_read (void *file, uint8_t *buf, int len) {
    mem_file_t *f = file;
    int left = f->size - f->pos;
    if (len > left) {
        len = left;
    }
    if (len <= 0) {
        return 0;
    }
    memcpy(buf, f->data + f->pos, len);
    f->pos += len;
    return len;
}

static int mem_close (void *file) {
    ((mem_file_t *) file)->open = false;
    return 0;
}

static filesystem_t mem_fs = {
    .thread_safe = true,
    .open = mem_open,
    .fstat = mem_fstat,
    .stat = mem_stat,
    .lseek = mem_lseek,
    .read = mem_read,
    .close = mem_close,
};

static void codec_init (void) {
    if (!codec_ready) {
        attach_filesystem(MEM_PREFIX, &mem_fs);
        video_register_codec(&h264_codec);
        codec_ready = true;
    }
}

/* ---------- on or off ---------- */

static bool enabled = true;     /* Settings > Video Previews (config.ini, the same for every profile) */

void video_preview_enable (bool on) {
    enabled = on;
    if (!on) {
        video_preview_stop();
    }
}

/* The clip, its pictures and the decoder need the Expansion Pak's memory. */
static bool previews_on (void) {
    return enabled && is_memory_expanded();
}

/* ---------- finding a clip ---------- */

bool video_preview_find (const char *storage_prefix, const char game_code[4], char *out, size_t size) {
    if (!previews_on()) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        char c = game_code[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!ok) {
            return false;   /* homebrew without a proper code: no folder to look in */
        }
    }

    /* The region's own folder first, then the one for every region. */
    const char *slash = (storage_prefix[0] && storage_prefix[strlen(storage_prefix) - 1] == '/') ? "" : "/";
    for (int letters = 4; letters >= 3; letters--) {
        char path[96];
        if (letters == 4) {
            snprintf(path, sizeof(path), "%s%smenu/metadata/%c/%c/%c/%c/" VIDEO_PREVIEW_FILE, storage_prefix, slash,
                game_code[0], game_code[1], game_code[2], game_code[3]);
        } else {
            snprintf(path, sizeof(path), "%s%smenu/metadata/%c/%c/%c/" VIDEO_PREVIEW_FILE, storage_prefix, slash,
                game_code[0], game_code[1], game_code[2]);
        }
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(out, size, "%s", path);
            return true;
        }
    }
    return false;
}

/* ---------- starting and stopping ---------- */

static void free_playback (void) {
    if (video || blitter.block) {
        rspq_wait();    /* nothing may still be drawing from its pictures */
    }
    if (blitter.block) {
        yuv_blitter_free(&blitter);
        blitter.block = NULL;
        blit_x = blit_y = blit_w = blit_h = -1;
    }
    if (video) {
        video_close(video);
        video = NULL;
        yuv_close();
    }
    free(hold);
    hold = NULL;
    hold_ready = false;
    looping = 0;
    behind = 0;
    have_frame = false;
}

static void free_clip (void) {
    if (load_file) {
        fclose(load_file);
        load_file = NULL;
    }
    free(clip_data);
    clip_data = NULL;
    free(seek_data);
    seek_data = NULL;
    seek_size = 0;
}

void video_preview_stop (void) {
    free_playback();
    free_clip();
    clip_size = clip_loaded = 0;
    want_path[0] = '\0';
    state = VP_OFF;
}

static void fail (const char *why) {
    debugf("Video preview: %s (%s)\n", why, want_path);
    free_playback();
    free_clip();
    clip_size = clip_loaded = 0;
    state = VP_FAILED;      /* keeps want_path, so it isn't tried again every frame */
}

/* Room for this much more, leaving the menu its reserve? If not, the cover
   stays and Settings > System > Memory says why. */
static bool room_for (int bytes) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    if ((heap.total - heap.used) >= bytes + RESERVE_BYTES) {
        return true;
    }
    memory_note_cut(MEMORY_CUT_VIDEO_PREVIEW);
    fail("not enough memory");
    return false;
}

/* The clip's seek table, if the card has one beside it ("preview.h264" ->
   "preview.seek"). Small, so it is read in one go. Without it the clip
   loops the slower way. */
static void load_seek_table (void) {
    char path[sizeof(want_path)];
    snprintf(path, sizeof(path), "%s", want_path);
    char *dot = strrchr(path, '.');
    if (!dot || (size_t) (dot - path) + sizeof(".seek") > sizeof(path)) {
        return;
    }
    strcpy(dot, ".seek");
    FILE *f = fopen(path, "rb");
    if (!f) {
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size >= 16 && size <= MAX_SEEK_BYTES) {
        seek_data = malloc(size);
        if (seek_data && fread(seek_data, 1, size, f) == (size_t) size) {
            seek_size = (int) size;
        } else {
            free(seek_data);
            seek_data = NULL;
        }
    }
    fclose(f);
}

/* One step of reading the clip into memory. */
/* When the frame being shown began (video_preview_update() runs as it is
   drawn), and how long a piece takes to read on this card, so pieces are
   only read when there is time before the next frame. */
static uint64_t frame_began_us = 0;
static uint32_t piece_us = 8000;
static uint64_t last_piece_ms = 0;

static void load_step (void) {
    if (!load_file) {
        load_file = fopen(want_path, "rb");
        if (!load_file) {
            fail("can't open");
            return;
        }
        fseek(load_file, 0, SEEK_END);
        long size = ftell(load_file);
        fseek(load_file, 0, SEEK_SET);
        if (size < 64 || size > MAX_CLIP_BYTES) {
            fail("too small or too big");
            return;
        }
        if (!room_for((int) size)) {
            return;
        }
        clip_data = malloc(size);
        if (!clip_data) {
            fail("out of memory");
            return;
        }
        clip_size = (int) size;
        clip_loaded = 0;
    }

    /* Read 64 KB at a time inside the frame, this took 57 ms a piece
       (the owner's card, 2026-11-10): 8 frames of 85-100 ms on opening Game
       info. Small pieces in the idle time keep those frames on time. */
    int want = clip_size - clip_loaded;
    if (want > LOAD_PIECE) {
        want = LOAD_PIECE;
    }
    uint64_t read_began = get_ticks_us();
    size_t got = fread(clip_data + clip_loaded, 1, want, load_file);
    piece_us = (piece_us * 3 + (uint32_t) (get_ticks_us() - read_began)) / 4;
    last_piece_ms = get_ticks_ms();
    if (got != (size_t) want) {
        fail("read error");
        return;
    }
    clip_loaded += want;

    if (clip_loaded == LOAD_STEP || clip_loaded == clip_size) {
        /* Check the header as soon as it is in. */
        video_clip_info_t check;
        video_clip_result_t result = video_clip_check(clip_data, clip_loaded, &check);
        if (result != VIDEO_CLIP_OK) {
            debugf("Video preview: header check %d, %dx%d\n", (int) result, check.width, check.height);
            fail("not a clip this player takes");
            return;
        }
        clip_w = check.width;
        clip_h = check.height;
    }
    if (clip_loaded == clip_size) {
        fclose(load_file);
        load_file = NULL;
        frame_count = video_clip_count_frames(clip_data, clip_size);
        load_seek_table();
        state = VP_WAITING;
    }
}

static void start_playing (uint64_t now) {
    if (!room_for(DECODER_BYTES(clip_w, clip_h))) {
        return;
    }
    codec_init();
    yuv_init();
    video = video_open(MEM_PREFIX MEM_NAME, &(video_parms_t) { .buffered_pics = BUFFERED_PICS });
    if (!video) {
        yuv_close();
        fail("the player would not open it");
        return;
    }
    info = video_get_info(video);
    if (info.width % 32 || info.width > VIDEO_CLIP_MAX_WIDTH || info.height > VIDEO_CLIP_MAX_HEIGHT || info.framerate < 1.0f) {
        fail("unexpected size or speed");
        return;
    }
    if (!video_next_frame(video)) {
        fail("no pictures");
        return;
    }
    debugf("Video preview: %dx%d at %.2f fps, %d KB\n", info.width, info.height, info.framerate, clip_size / 1024);
    have_frame = true;
    memset(&stats, 0, sizeof(stats));
    stats.width = info.width;
    stats.height = info.height;
    stats.fps = info.framerate;
    stats.frames_shown = 1;
    frames_shown = 0;
    clip_seconds = 0.0;
    fade_since_ms = now;
    state = VP_PLAYING;
}

/* Keep a copy of the clip's last picture, to show while the first one is
   decoded again (see advance). The RSP reads it, so it is written back from
   the CPU's cache. */
static void hold_last_picture (void) {
    yuv_frame_t frame = video_get_frame(video);
    int y_size = info.width * info.height;
    int c_size = y_size / 4;
    if (!hold) {
        hold = memalign(16, y_size + 2 * c_size);
        if (!hold) {
            return;
        }
    }
    memcpy(hold, frame.y.buffer, y_size);
    memcpy(hold + y_size, frame.u.buffer, c_size);
    memcpy(hold + y_size + c_size, frame.v.buffer, c_size);
    data_cache_hit_writeback(hold, y_size + 2 * c_size);
    hold_ready = true;
}

/* Round to the start without stopping: show the copy of the last picture,
   go back to the start of the stream (only a change of place, thanks to the
   seek table), and let the first picture decode in the idle time of the
   next few frames. */
static bool loop_smoothly (void) {
    if (!seek_data) {
        return false;   /* no table: video_seek would decode the first picture at once anyway */
    }
    if (!hold_ready) {
        debugf("Video preview: no held picture at the end (%d of %d counted)\n", frames_shown, frame_count);
        return false;
    }
    if (video_seek(video, 0) != 0) {
        debugf("Video preview: could not seek to the start\n");
        return false;
    }
    looping = LOOP_FRAMES;
    return true;
}

/* The way round when that can't be done: decode the first picture at once. */
static bool loop_at_once (void) {
    uint64_t began = get_ticks_us();
    video_rewind(video);
    if (!video_next_frame(video)) {
        return false;
    }
    uint32_t took = (uint32_t) (get_ticks_us() - began);
    stats.loop_us_max = (took > stats.loop_us_max) ? took : stats.loop_us_max;
    return true;
}

/* One picture on. False at the end of the clip (handled here). */
static bool next_picture (void) {
    uint64_t began = get_ticks_us();
    bool more = video_next_frame(video);
    uint32_t took = (uint32_t) (get_ticks_us() - began);
    stats.next_us_max = (took > stats.next_us_max) ? took : stats.next_us_max;
    /* Over a millisecond means it wasn't ready: the idle time isn't keeping
       up, so some decoding is done inside the frame for a while. */
    behind = (took > 1000) ? 30 : (behind > 0 ? behind - 1 : 0);
    return more;
}

/* Move the clip on to where its clock says it should be. */
static void advance (uint64_t step_ms) {
    if (looping > 0) {
        /* Going round: the held picture is up while the first decodes. */
        if (--looping > 0) {
            return;
        }
        if (!video_next_frame(video)) {
            fail("no pictures after looping");
            return;
        }
        stats.loops++;
        stats.frames_shown++;
        frames_shown = 0;
        clip_seconds = 0.0;
        return;
    }

    clip_seconds += step_ms / 1000.0;
    int target = (int) (clip_seconds * info.framerate);

    /* At most two pictures a frame: a menu frame (30 a second) is never
       more than two of the clip's behind unless decoding has fallen behind,
       and then the clip waits rather than the menu. */
    for (int steps = 0; frames_shown < target && steps < 2; steps++) {
        if (!next_picture()) {
            if (loop_smoothly()) {
                return;
            }
            if (!loop_at_once()) {
                fail("no pictures after looping");
                return;
            }
            stats.loops++;
            stats.frames_shown++;
            frames_shown = 0;
            clip_seconds = 0.0;
            return;
        }
        frames_shown++;
        stats.frames_shown++;
        if (frame_count > 0 && frames_shown >= frame_count - HOLD_FROM_END) {
            hold_last_picture();    /* the last few, in case the decoder gives out one or two fewer than counted */
        }
    }
    if (frames_shown < target) {
        clip_seconds = frames_shown / (double) info.framerate;
        stats.late++;
    }

    if (behind > 0) {
        /* Decode ahead inside the frame too, while the idle time falls short. */
        uint64_t began = get_ticks_us();
        while (get_ticks_us() - began < FRAME_BUDGET_US && video_poll(video) > 0) {
        }
    }
}

void video_preview_update (const char *path) {
    uint64_t now = get_ticks_ms();

    if (!path || !path[0] || !previews_on()) {
        if (state != VP_OFF) {
            video_preview_stop();
        }
        return;
    }

    if (strcmp(path, want_path) != 0) {
        video_preview_stop();
        snprintf(want_path, sizeof(want_path), "%s", path);
        state = VP_LOADING;
        wanted_ms = now;
        last_update_ms = now;
    }

    frame_began_us = get_ticks_us();
    uint64_t step = now - last_update_ms;
    if (step > AWAY_MS) {
        /* Shown again after a while (the other pictures, a pop-up): fade
           in again from where it was. */
        fade_since_ms = now;
    }
    if (step > MAX_STEP_MS) {
        step = MAX_STEP_MS;
    }
    last_update_ms = now;

    uint64_t began = get_ticks_us();
    switch (state) {
        case VP_LOADING:
            break;      /* read in the idle time between frames (video_preview_poll()) */
        case VP_WAITING:
            if (now - wanted_ms >= START_DELAY_MS) {
                start_playing(now);
            }
            break;
        case VP_PLAYING:
            advance(step);
            stats.update_us = (uint32_t) (get_ticks_us() - began);
            stats.update_us_max = (stats.update_us > stats.update_us_max) ? stats.update_us : stats.update_us_max;
            break;
        default:
            break;
    }
}

const video_preview_stats_t *video_preview_stats (void) {
    return &stats;
}

bool video_preview_showing (void) {
    return state == VP_PLAYING && video && (get_ticks_ms() - last_update_ms) <= AWAY_MS;
}

void video_preview_poll (void) {
    if (state == VP_LOADING && (get_ticks_ms() - last_update_ms) <= AWAY_MS) {
        /* One piece, if it should be done before the next frame is due;
           the main loop comes round again soon. Reading regardless made
           the last piece run past the refresh: every frame 60 ms. */
        uint64_t due_us = frame_began_us + frame_rate_frame_us() - LOAD_MARGIN_US;
        if (get_ticks_us() + piece_us <= due_us || (get_ticks_ms() - last_piece_ms) > LOAD_STALL_MS) {
            load_step();
        }
        return;
    }
    if (state != VP_PLAYING || !video) {
        return;
    }
    if (get_ticks_ms() - last_update_ms > AWAY_MS) {
        return;     /* not being shown: don't spend time on it */
    }
    if (video_poll(video) > 0) {    /* one slice; the main loop comes round again soon */
        stats.idle_slices++;
    }
}

bool video_preview_fit (int w, int h, int *shown_w, int *shown_h) {
    if (state != VP_PLAYING || !have_frame) {
        return false;
    }
    float aspect = (info.aspect_ratio > 0.1f) ? info.aspect_ratio : ((float) info.width / info.height);
    int fw = w, fh = (int) (w / aspect + 0.5f);
    if (fh > h) {
        fh = h;
        fw = (int) (h * aspect + 0.5f);
    }
    *shown_w = fw;
    *shown_h = fh;
    return true;
}

float video_preview_fade (void) {
    if (state != VP_PLAYING) {
        return 0.0f;
    }
    uint64_t since = get_ticks_ms() - fade_since_ms;
    return (since >= FADE_MS) ? 1.0f : (since / (float) FADE_MS);
}

bool video_preview_draw (int x, int y, int w, int h) {
    int fw, fh;
    if (!video_preview_fit(w, h, &fw, &fh)) {
        return false;
    }
    int x0 = x + (w - fw) / 2;
    int y0 = y + (h - fh) / 2;

    /* The blitter is recorded once for a place and size, then replayed. */
    if (!blitter.block || x0 != blit_x || y0 != blit_y || fw != blit_w || fh != blit_h) {
        if (blitter.block) {
            yuv_blitter_free(&blitter);
        }
        blitter = yuv_blitter_new(info.width, info.height, x0, y0, &(rdpq_blitparms_t) {
            .scale_x = fw / (float) info.width,
            .scale_y = fh / (float) info.height,
            .filtering = true,
        }, &info.colorspace);
        blit_x = x0;
        blit_y = y0;
        blit_w = fw;
        blit_h = fh;
    }

    yuv_frame_t frame;
    if (looping > 0 && hold_ready) {
        int y_size = info.width * info.height;
        int cw = info.width / 2, ch = info.height / 2;
        frame = (yuv_frame_t) {
            .y = surface_make(hold, FMT_I8, info.width, info.height, info.width),
            .u = surface_make(hold + y_size, FMT_I8, cw, ch, cw),
            .v = surface_make(hold + y_size + cw * ch, FMT_I8, cw, ch, cw),
        };
    } else {
        frame = video_get_frame(video);
    }
    rdpq_mode_push();
        yuv_blitter_run(&blitter, &frame);
    rdpq_mode_pop();
    return true;
}
