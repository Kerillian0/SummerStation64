/**
 * @file shot_png.c
 * @brief Writes a PNG picture a line at a time, compressed, in little memory.
 */

#include <stdlib.h>
#include <string.h>

#include "shot_png.h"

#define OUT_SIZE    (8192)      /* compressed bytes gathered before each IDAT chunk */
#define MIN_MATCH   (3)
#define MAX_MATCH   (258)

typedef struct {
    FILE *f;
    bool ok;
    uint8_t out[OUT_SIZE];
    int used;
    uint32_t bits;              /* not yet written, lowest first */
    int bit_count;
    uint32_t adler_a, adler_b;
    uint8_t *rows[3];           /* this line and the two above, each with its filter byte first */
} writer_t;

static uint32_t crc_table[256];

static void crc_table_make (void) {
    if (crc_table[1]) {
        return;
    }
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc_table[n] = c;
    }
}

static uint32_t crc_add (uint32_t crc, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void put32 (uint8_t *at, uint32_t value) {
    at[0] = value >> 24;
    at[1] = value >> 16;
    at[2] = value >> 8;
    at[3] = value;
}

/* A PNG chunk written in one go. */
static void chunk (writer_t *w, const char *type, const uint8_t *data, uint32_t length) {
    uint8_t head[8];
    put32(head, length);
    memcpy(head + 4, type, 4);
    uint32_t crc = crc_add(0xFFFFFFFFu, (const uint8_t *) type, 4);
    crc = crc_add(crc, data, length);
    uint8_t tail[4];
    put32(tail, crc ^ 0xFFFFFFFFu);
    if (fwrite(head, 1, 8, w->f) != 8 || (length && fwrite(data, 1, length, w->f) != length) || fwrite(tail, 1, 4, w->f) != 4) {
        w->ok = false;
    }
}

static void flush_out (writer_t *w) {
    if (w->used) {
        chunk(w, "IDAT", w->out, w->used);
        w->used = 0;
    }
}

static void put_byte (writer_t *w, uint8_t b) {
    w->out[w->used++] = b;
    if (w->used == OUT_SIZE) {
        flush_out(w);
    }
}

/* Bits as deflate packs them: the first bit lowest in each byte. */
static void put_bits (writer_t *w, uint32_t value, int count) {
    w->bits |= value << w->bit_count;
    w->bit_count += count;
    while (w->bit_count >= 8) {
        put_byte(w, w->bits & 0xFF);
        w->bits >>= 8;
        w->bit_count -= 8;
    }
}

/* A Huffman code goes in highest bit first. */
static void put_code (writer_t *w, uint32_t code, int count) {
    uint32_t reversed = 0;
    for (int i = 0; i < count; i++) {
        reversed = (reversed << 1) | ((code >> i) & 1);
    }
    put_bits(w, reversed, count);
}

/* deflate's fixed codes for a literal byte, a length or the end (RFC 1951, 3.2.6). */
static void put_symbol (writer_t *w, int s) {
    if (s < 144) {
        put_code(w, 0x30 + s, 8);
    } else if (s < 256) {
        put_code(w, 0x190 + (s - 144), 9);
    } else if (s < 280) {
        put_code(w, s - 256, 7);
    } else {
        put_code(w, 0xC0 + (s - 280), 8);
    }
}

static const uint16_t length_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t length_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t distance_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t distance_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static void put_match (writer_t *w, int length, int distance) {
    int l = 28;
    while (length_base[l] > length) {
        l--;
    }
    put_symbol(w, 257 + l);
    put_bits(w, length - length_base[l], length_extra[l]);
    int d = 29;
    while (distance_base[d] > distance) {
        d--;
    }
    put_code(w, d, 5);
    put_bits(w, distance - distance_base[d], distance_extra[d]);
}

/* How many bytes from `at` on are the same as those `back` bytes earlier in
   the line (or the same place in a line above), up to the end of the line. */
static int same_run (const uint8_t *line, const uint8_t *earlier, int at, int end) {
    int n = 0;
    while (at + n < end && n < MAX_MATCH && line[at + n] == earlier[at + n]) {
        n++;
    }
    return n;
}

/* One line (filter byte included): each byte either as itself or as part of
   a repeat of the bytes 3, 6 or 12 back (one, two or four pixels) or of
   the one or two lines above. */
static void put_line (writer_t *w, int y, int row_bytes) {
    const uint8_t *line = w->rows[0];
    int i = 0;
    while (i < row_bytes) {
        int best = 0, best_distance = 0;
        static const int backs[3] = { 3, 6, 12 };
        for (int k = 0; k < 3; k++) {
            int back = backs[k];
            if (i - back >= 1) {
                int n = same_run(line, line - back, i, row_bytes);
                if (n > best) {
                    best = n;
                    best_distance = back;
                }
            }
        }
        for (int up = 1; up <= 2 && up <= y; up++) {
            int n = same_run(line, w->rows[up], i, row_bytes);
            if (n > best) {
                best = n;
                best_distance = up * row_bytes;
            }
        }
        if (best >= MIN_MATCH) {
            put_match(w, best, best_distance);
            i += best;
        } else {
            put_symbol(w, line[i]);
            i++;
        }
    }
}

bool shot_png_write (FILE *f, int width, int height, shot_png_line_t line, void *context) {
    if (width < 1 || height < 1 || width > SHOT_PNG_MAX_WIDTH) {
        return false;
    }
    int row_bytes = 1 + (width * 3);
    writer_t *w = calloc(1, sizeof(writer_t));
    uint8_t *rows = malloc(3 * row_bytes);
    if (!w || !rows) {
        free(w);
        free(rows);
        return false;
    }
    crc_table_make();
    w->f = f;
    w->ok = true;
    w->adler_a = 1;
    for (int r = 0; r < 3; r++) {
        w->rows[r] = rows + (r * row_bytes);
    }

    static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (fwrite(signature, 1, 8, f) != 8) {
        w->ok = false;
    }
    uint8_t header[13];
    put32(header, width);
    put32(header + 4, height);
    header[8] = 8;      /* 8 bits per colour */
    header[9] = 2;      /* red, green, blue */
    header[10] = 0;     /* deflate */
    header[11] = 0;     /* the usual line filters */
    header[12] = 0;     /* not interlaced */
    chunk(w, "IHDR", header, sizeof(header));

    /* The picture data: a zlib stream holding one deflate block with the
       fixed codes, spread over as many IDAT chunks as it takes. */
    put_byte(w, 0x78);
    put_byte(w, 0x01);
    put_bits(w, 1, 1);      /* the last block */
    put_bits(w, 1, 2);      /* fixed codes */
    for (int y = 0; y < height && w->ok; y++) {
        uint8_t *oldest = w->rows[2];
        w->rows[2] = w->rows[1];
        w->rows[1] = w->rows[0];
        w->rows[0] = oldest;
        w->rows[0][0] = 0;  /* no filter */
        line(y, w->rows[0] + 1, context);
        for (int i = 0; i < row_bytes; i++) {
            w->adler_a = (w->adler_a + w->rows[0][i]) % 65521;
            w->adler_b = (w->adler_b + w->adler_a) % 65521;
        }
        put_line(w, y, row_bytes);
    }
    put_symbol(w, 256);     /* end of the block */
    if (w->bit_count) {
        put_bits(w, 0, 8 - w->bit_count);
    }
    uint32_t adler = (w->adler_b << 16) | w->adler_a;
    put_byte(w, adler >> 24);
    put_byte(w, adler >> 16);
    put_byte(w, adler >> 8);
    put_byte(w, adler);
    flush_out(w);
    chunk(w, "IEND", NULL, 0);

    bool ok = w->ok;
    free(rows);
    free(w);
    return ok;
}
