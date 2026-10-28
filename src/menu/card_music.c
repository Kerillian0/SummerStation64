#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "card_music.h"
#include "sound.h"

#define HEADER_SIZE     (32)
#define RING_4MB        (64 * 1024)     /* about 0.7 s of 22 kHz stereo */
#define RING_8MB        (256 * 1024)    /* about 3 s */
#define PIECE           (16 * 1024)     /* read from the card at most once a frame */

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static int fd = -1;
static waveform_t wave;
static uint8_t *ring;
static int ring_size;
static int ring_start;      /* first byte not yet played */
static int ring_count;      /* bytes waiting in the ring */
static int frame_bytes;     /* one sample of every channel */
static uint32_t data_start; /* where the samples start in the file */
static int data_bytes;
static int file_pos;        /* next byte to read, from data_start */
static int play_pos;        /* next sample the mixer gets */

static int be32 (const uint8_t *p) {
    return (int) (((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3]);
}

/* One read of up to `most` bytes into the ring, going back to the start at the file's end. */
static void read_piece (int most) {
    int end = (ring_start + ring_count) % ring_size;
    int room = MIN(ring_size - ring_count, ring_size - end);
    int n = MIN(MIN(most, room), data_bytes - file_pos);
    if (n <= 0) {
        return;
    }
    int got = read(fd, ring + end, n);
    if (got <= 0) {
        return;
    }
    ring_count += got;
    file_pos += got;
    if (file_pos >= data_bytes) {
        file_pos = 0;
        lseek(fd, data_start, SEEK_SET);
    }
}

/* Start again from sample `pos` (the start, or a jump the mixer asked for). */
static void restart_at (int pos) {
    ring_start = 0;
    ring_count = 0;
    play_pos = pos;
    file_pos = pos * frame_bytes;
    lseek(fd, data_start + file_pos, SEEK_SET);
}

static void wave_read (void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
    (void) ctx;
    if (seeking && wpos != play_pos % wave.len) {
        restart_at(wpos);
    }
    while (wlen > 0) {
        int n = MIN(wlen, SAMPLEBUFFER_MARGIN_UNITS);
        int bytes = n * frame_bytes;
        /* The ring ran dry (a long frame): read what is needed now. */
        for (int tries = 0; ring_count < bytes && tries < 4; tries++) {
            read_piece(MAX(bytes - ring_count, 4096));
        }
        uint8_t *out = samplebuffer_append(sbuf, n);
        int have = MIN(bytes, ring_count);
        int first = MIN(have, ring_size - ring_start);
        memcpy(out, ring + ring_start, first);
        memcpy(out + first, ring, have - first);
        memset(out + have, 0, bytes - have);    /* silence rather than noise if the card failed */
        ring_start = (ring_start + have) % ring_size;
        ring_count -= have;
        play_pos = (play_pos + n) % wave.len;
        wlen -= n;
    }
}

bool card_music_open (const char *path) {
    card_music_close();
    int f = open(path, O_RDONLY);
    if (f < 0) {
        return false;
    }
    uint8_t h[HEADER_SIZE];
    if (read(f, h, sizeof(h)) != sizeof(h) || memcmp(h, "WV64", 4) != 0
        || h[5] != 0 || h[7] != 16 || h[6] < 1 || h[6] > 2) {
        close(f);   /* compressed, or 8-bit: libdragon streams it */
        return false;
    }
    int size = (get_memory_size() > 0x400000) ? RING_8MB : RING_4MB;
    ring = malloc(size);
    if (!ring) {
        close(f);
        debugf("card music: no memory for its %d KB, streamed as before\n", size / 1024);
        return false;
    }
    fd = f;
    ring_size = size;
    frame_bytes = h[6] * 2;
    data_start = (uint32_t) be32(h + 24);
    int len = be32(h + 12);
    data_bytes = len * frame_bytes;
    memset(&wave, 0, sizeof(wave));
    wave.name = "music";
    wave.bits = 16;
    wave.channels = h[6];
    wave.frequency = be32(h + 8);
    wave.len = len;
    wave.loop_len = len;
    wave.read = wave_read;
    restart_at(0);
    read_piece(PIECE);
    debugf("card music: %s, read ahead %d KB at a time into %d KB\n", path, PIECE / 1024, size / 1024);
    return true;
}

waveform_t *card_music_wave (void) {
    return &wave;
}

void card_music_fill (void) {
    if (fd >= 0 && ring_size - ring_count >= PIECE) {
        read_piece(PIECE);
    }
}

void card_music_close (void) {
    if (fd < 0) {
        return;
    }
    for (int ch = 0; ch < MIXER_MAX_CHANNELS; ch++) {
        if (mixer_ch_playing_waveform(ch) == &wave) {
            mixer_ch_stop(ch);
        }
    }
    rspq_highpri_sync();
    close(fd);
    fd = -1;
    free(ring);
    ring = NULL;
}
