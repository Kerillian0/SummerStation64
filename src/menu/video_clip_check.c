#include <string.h>

#include "video_clip_check.h"

/* The sequence header's bits, with the 0x03 bytes the encoder slips in after
   two zero bytes ("emulation prevention") taken out again. */
#define SPS_MAX_BYTES   (64)

typedef struct {
    uint8_t bytes[SPS_MAX_BYTES];
    int size;
    int bit;
    bool overrun;
} bits_t;

static int read_bit (bits_t *b) {
    if (b->bit >= b->size * 8) {
        b->overrun = true;
        return 0;
    }
    int v = (b->bytes[b->bit >> 3] >> (7 - (b->bit & 7))) & 1;
    b->bit++;
    return v;
}

static uint32_t read_bits (bits_t *b, int n) {
    uint32_t v = 0;
    while (n-- > 0) {
        v = (v << 1) | (uint32_t) read_bit(b);
    }
    return v;
}

/* Exp-Golomb, unsigned. Anything over 31 leading zeros is not a real header. */
static uint32_t read_ue (bits_t *b) {
    int zeros = 0;
    while (!read_bit(b)) {
        if (b->overrun || ++zeros > 31) {
            b->overrun = true;
            return 0;
        }
    }
    return ((1u << zeros) - 1) + read_bits(b, zeros);
}

static int32_t read_se (bits_t *b) {
    uint32_t v = read_ue(b);
    return (v & 1) ? (int32_t) ((v + 1) / 2) : -(int32_t) (v / 2);
}

static void skip_scaling_list (bits_t *b, int count) {
    int last = 8, next = 8;
    for (int i = 0; i < count && !b->overrun; i++) {
        if (next != 0) {
            next = (last + read_se(b) + 256) % 256;
        }
        last = (next == 0) ? last : next;
    }
}

video_clip_result_t video_clip_check (const uint8_t *data, size_t size, video_clip_info_t *info) {
    /* A start code (00 00 01, or 00 00 00 01) and then a sequence header
       (NAL type 7), which every clip from the encoder starts with. */
    size_t at;
    if (size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 1) {
        at = 3;
    } else if (size >= 5 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        at = 4;
    } else {
        return VIDEO_CLIP_NOT_H264;
    }
    if ((data[at] & 0x80) || (data[at] & 0x1F) != 7) {
        return VIDEO_CLIP_NOT_H264;
    }
    at++;

    bits_t b = { .size = 0, .bit = 0, .overrun = false };
    int zeros = 0;
    while (at < size && b.size < SPS_MAX_BYTES) {
        uint8_t c = data[at++];
        if (zeros >= 2 && c == 0x03) {
            zeros = 0;          /* emulation prevention: not part of the header */
            continue;
        }
        if (zeros >= 2 && c <= 0x01) {
            break;              /* the next start code: the header has ended */
        }
        zeros = (c == 0) ? zeros + 1 : 0;
        b.bytes[b.size++] = c;
    }

    uint32_t profile = read_bits(&b, 8);
    read_bits(&b, 8);           /* constraint flags */
    read_bits(&b, 8);           /* level */
    if (read_ue(&b) > 31) {     /* sequence header id */
        return VIDEO_CLIP_BAD_HEADER;
    }

    if (profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 ||
        profile == 83 || profile == 86 || profile == 118 || profile == 128 || profile == 138 ||
        profile == 139 || profile == 134 || profile == 135) {
        /* High profiles carry the color format first: only 4:2:0, 8 bits. */
        uint32_t chroma = read_ue(&b);
        if (chroma != 1) {
            return VIDEO_CLIP_UNSUPPORTED;
        }
        if (read_ue(&b) != 0 || read_ue(&b) != 0) {    /* bit depths */
            return VIDEO_CLIP_UNSUPPORTED;
        }
        read_bit(&b);           /* lossless bypass */
        if (read_bit(&b)) {     /* scaling matrices */
            for (int i = 0; i < 8 && !b.overrun; i++) {
                if (read_bit(&b)) {
                    skip_scaling_list(&b, (i < 6) ? 16 : 64);
                }
            }
        }
    }

    if (read_ue(&b) > 12) {     /* log2 of the largest frame number, less 4 */
        return VIDEO_CLIP_BAD_HEADER;
    }
    uint32_t poc_type = read_ue(&b);
    if (poc_type == 0) {
        if (read_ue(&b) > 12) {
            return VIDEO_CLIP_BAD_HEADER;
        }
    } else if (poc_type == 1) {
        read_bit(&b);
        read_se(&b);
        read_se(&b);
        uint32_t cycle = read_ue(&b);
        if (cycle > 255) {
            return VIDEO_CLIP_BAD_HEADER;
        }
        for (uint32_t i = 0; i < cycle && !b.overrun; i++) {
            read_se(&b);
        }
    } else if (poc_type != 2) {
        return VIDEO_CLIP_BAD_HEADER;
    }

    if (read_ue(&b) > 16) {     /* reference frames */
        return VIDEO_CLIP_BAD_HEADER;
    }
    read_bit(&b);               /* gaps in frame numbers allowed */
    uint32_t mbs_wide = read_ue(&b) + 1;
    uint32_t mbs_high = read_ue(&b) + 1;
    bool frames_only = read_bit(&b);

    if (b.overrun) {
        return VIDEO_CLIP_BAD_HEADER;
    }
    if (!frames_only) {
        return VIDEO_CLIP_UNSUPPORTED;      /* interlaced */
    }

    int width = (int) (mbs_wide * 16);
    int height = (int) (mbs_high * 16);
    if (info) {
        info->width = width;
        info->height = height;
    }
    if (width < 32 || height < 16 || width > VIDEO_CLIP_MAX_WIDTH || height > VIDEO_CLIP_MAX_HEIGHT || (width % 32) != 0) {
        return VIDEO_CLIP_BAD_SIZE;
    }
    return VIDEO_CLIP_OK;
}

int video_clip_count_frames (const uint8_t *data, size_t size) {
    /* A picture starts with a slice (NAL type 1, or 5 for a keyframe) whose
       first macroblock is 0: first_mb_in_slice is the first thing in the
       slice header, and 0 is written as a single 1 bit. */
    int frames = 0;
    for (size_t i = 0; i + 4 < size; i++) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            uint8_t type = data[i + 3] & 0x1F;
            if ((type == 1 || type == 5) && (data[i + 4] & 0x80)) {
                frames++;
            }
            i += 2;
        }
    }
    return frames;
}
