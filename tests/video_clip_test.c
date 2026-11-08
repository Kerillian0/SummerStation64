/* The check made on a preview clip before the H.264 player is given it
   (src/menu/video_clip_check.c). Built and run by tests/run.sh. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_clip_check.h"

static int failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

/* The start of a real clip from scripts/make_preview_clip.py (192x144, Main
   profile): its sequence header, which has an emulation-prevention byte in
   it (00 00 03), then the picture parameters. */
static const uint8_t real_clip[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x4d, 0x40, 0x0c, 0xd8, 0x90, 0xc1, 0x3b, 0x01, 0x6e, 0x02, 0x02,
    0x02, 0x80, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x14, 0x07, 0x8a, 0x15, 0x13, 0x00, 0x00,
    0x00, 0x01, 0x68, 0xc8, 0x8c, 0x24, 0xb2, 0x00,
};

/* A small bit writer, to make sequence headers of other kinds. */
typedef struct {
    uint8_t bytes[64];
    int bit;
} writer_t;

static void put_bit (writer_t *w, int v) {
    if (v) {
        w->bytes[w->bit >> 3] |= (uint8_t) (0x80 >> (w->bit & 7));
    }
    w->bit++;
}

static void put_bits (writer_t *w, uint32_t v, int n) {
    while (n-- > 0) {
        put_bit(w, (v >> n) & 1);
    }
}

static void put_ue (writer_t *w, uint32_t v) {
    uint32_t x = v + 1;
    int len = 0;
    while ((x >> len) > 1) {
        len++;
    }
    put_bits(w, 0, len);
    put_bits(w, x, len + 1);
}

/* Start code, NAL header, then the fields up to frame_mbs_only_flag. */
static size_t make_sps (uint8_t *out, int profile, int chroma, int mbs_wide, int mbs_high, int frames_only) {
    writer_t w;
    memset(&w, 0, sizeof(w));
    put_bits(&w, profile, 8);
    put_bits(&w, 0, 8);         /* constraints */
    put_bits(&w, 30, 8);        /* level */
    put_ue(&w, 0);              /* sps id */
    if (profile == 100) {
        put_ue(&w, chroma);
        put_ue(&w, 0);
        put_ue(&w, 0);
        put_bit(&w, 0);
        put_bit(&w, 0);
    }
    put_ue(&w, 0);              /* log2_max_frame_num - 4 */
    put_ue(&w, 0);              /* poc type 0 */
    put_ue(&w, 2);              /* log2_max_poc_lsb - 4 */
    put_ue(&w, 1);              /* reference frames */
    put_bit(&w, 0);
    put_ue(&w, mbs_wide - 1);
    put_ue(&w, mbs_high - 1);
    put_bit(&w, frames_only);
    put_bit(&w, 1);             /* stop bit, more or less */

    static const uint8_t head[] = { 0, 0, 0, 1, 0x67 };
    memcpy(out, head, sizeof(head));
    int n = (w.bit + 7) / 8 + 1;
    memcpy(out + sizeof(head), w.bytes, n);
    return sizeof(head) + n;
}

int main (int argc, char **argv) {
    video_clip_info_t info;

    /* Clips named on the command line (made by make_preview_clip.py) must pass too. */
    for (int i = 1; i < argc; i++) {
        static uint8_t head[4096];
        FILE *f = fopen(argv[i], "rb");
        size_t got = f ? fread(head, 1, sizeof(head), f) : 0;
        if (f) {
            fclose(f);
        }
        if (strchr(argv[i], '=')) {
            continue;   /* a FILE=COUNT, below */
        }
        memset(&info, 0, sizeof(info));
        video_clip_result_t r = video_clip_check(head, got, &info);
        EXPECT(r == VIDEO_CLIP_OK, "%s: result %d, %dx%d", argv[i], (int) r, info.width, info.height);
    }

    /* A clip's picture count: given as FILE=COUNT (from ffprobe -count_frames). */
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        static uint8_t whole[4 * 1024 * 1024];
        FILE *f = fopen(argv[i], "rb");
        size_t got = f ? fread(whole, 1, sizeof(whole), f) : 0;
        if (f) {
            fclose(f);
        }
        int want = atoi(eq + 1), have = video_clip_count_frames(whole, got);
        EXPECT(have == want, "%s: %d pictures counted, ffprobe says %d", argv[i], have, want);
        *eq = '=';
    }

    /* Two pictures: a keyframe and a following picture, each in two slices. */
    static const uint8_t two[] = {
        0, 0, 0, 1, 0x67, 0x4d,             /* sequence header: not a picture */
        0, 0, 0, 1, 0x65, 0x88, 0x10,       /* keyframe slice, first macroblock 0 */
        0, 0, 1, 0x65, 0x40, 0x10,          /* keyframe, a later slice */
        0, 0, 0, 1, 0x41, 0x9a, 0x10,       /* next picture */
        0, 0, 1, 0x41, 0x20, 0x10, 0x00,    /* its second slice */
    };
    EXPECT(video_clip_count_frames(two, sizeof(two)) == 2, "two pictures, got %d", video_clip_count_frames(two, sizeof(two)));

    memset(&info, 0, sizeof(info));
    EXPECT(video_clip_check(real_clip, sizeof(real_clip), &info) == VIDEO_CLIP_OK, "real clip accepted");
    EXPECT(info.width == 192 && info.height == 144, "real clip is 192x144, got %dx%d", info.width, info.height);

    /* The same with a three-byte start code. */
    EXPECT(video_clip_check(real_clip + 1, sizeof(real_clip) - 1, &info) == VIDEO_CLIP_OK, "3-byte start code");

    /* Cut short in the middle of the header. */
    EXPECT(video_clip_check(real_clip, 9, &info) == VIDEO_CLIP_BAD_HEADER, "truncated header");

    /* Not H.264 at all, or a stream that doesn't start with its header. */
    static const uint8_t png[] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    EXPECT(video_clip_check(png, sizeof(png), NULL) == VIDEO_CLIP_NOT_H264, "a PNG");
    static const uint8_t slice_first[] = { 0, 0, 0, 1, 0x65, 0x88, 0x84, 0x00 };
    EXPECT(video_clip_check(slice_first, sizeof(slice_first), NULL) == VIDEO_CLIP_NOT_H264, "picture before header");
    EXPECT(video_clip_check(real_clip, 3, NULL) == VIDEO_CLIP_NOT_H264, "three bytes");
    EXPECT(video_clip_check(NULL, 0, NULL) == VIDEO_CLIP_NOT_H264, "nothing");

    uint8_t sps[80];
    size_t n;

    n = make_sps(sps, 77, 1, 10, 8, 1);     /* 160x128 */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_OK && info.width == 160 && info.height == 128,
        "160x128 Main, got %dx%d", info.width, info.height);

    n = make_sps(sps, 66, 1, 20, 15, 1);    /* 320x240, the largest */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_OK, "320x240 Baseline");

    n = make_sps(sps, 77, 1, 40, 30, 1);    /* 640x480 */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_BAD_SIZE && info.width == 640, "640x480 too big");

    n = make_sps(sps, 77, 1, 11, 8, 1);     /* 176 wide: not a multiple of 32 */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_BAD_SIZE, "176 wide");

    n = make_sps(sps, 77, 1, 12, 9, 0);     /* interlaced */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_UNSUPPORTED, "interlaced");

    n = make_sps(sps, 100, 1, 12, 9, 1);    /* High, 4:2:0 */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_OK && info.width == 192, "High 4:2:0");

    n = make_sps(sps, 100, 2, 12, 9, 1);    /* High, 4:2:2 */
    EXPECT(video_clip_check(sps, n, &info) == VIDEO_CLIP_UNSUPPORTED, "High 4:2:2");

    if (failures == 0) {
        printf("video clip check: all passed\n");
    }
    return failures ? 1 : 0;
}
