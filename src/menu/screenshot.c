/**
 * @file screenshot.c
 * @brief Saves the screen as a picture on the SD card.
 */

#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "menu_features.h"
#include "safe_file.h"
#include "screenshot.h"
#include "sound.h"
#include "utils/fs.h"

#define FOLDER      "sd:/menu/screenshots"
#define TEMP_PATH   FOLDER "/shot.tmp"
#define MAX_WIDTH   (640)

static bool wanted = false;
static int last_number = 0;     /* the highest picture number seen or written */

/* One line of the picture as it is stored: a stored deflate block's 5-byte
   header, the line's filter byte, then red, green and blue for each pixel. */
static uint8_t line[5 + 1 + (MAX_WIDTH * 3)];

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

/* A PNG chunk that is written in one go. */
static void chunk (FILE *f, const char *type, const uint8_t *data, uint32_t length) {
    uint8_t head[8];
    put32(head, length);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    if (length) {
        fwrite(data, 1, length, f);
    }
    uint32_t crc = crc_add(0xFFFFFFFFu, (const uint8_t *) type, 4);
    crc = crc_add(crc, data, length);
    uint8_t tail[4];
    put32(tail, crc ^ 0xFFFFFFFFu);
    fwrite(tail, 1, 4, f);
}

static bool write_png (const char *path, surface_t *display) {
    int width = display->width;
    int height = display->height;
    if (width > MAX_WIDTH || surface_get_format(display) != FMT_RGBA16) {
        return false;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    crc_table_make();

    static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(signature, 1, 8, f);

    uint8_t header[13];
    put32(header, width);
    put32(header + 4, height);
    header[8] = 8;      /* 8 bits per colour */
    header[9] = 2;      /* red, green, blue */
    header[10] = 0;     /* deflate */
    header[11] = 0;     /* the usual line filters */
    header[12] = 0;     /* not interlaced */
    chunk(f, "IHDR", header, sizeof(header));

    /* The picture data: a zlib stream of stored (uncompressed) blocks, one
       per line, so nothing has to be held but one line. */
    uint32_t row_bytes = 1 + (width * 3);
    uint32_t data_length = 2 + (height * (5 + row_bytes)) + 4;
    uint8_t head[8];
    put32(head, data_length);
    memcpy(head + 4, "IDAT", 4);
    fwrite(head, 1, 8, f);
    uint32_t crc = crc_add(0xFFFFFFFFu, (const uint8_t *) "IDAT", 4);

    static const uint8_t zlib_header[2] = { 0x78, 0x01 };
    fwrite(zlib_header, 1, 2, f);
    crc = crc_add(crc, zlib_header, 2);

    uint32_t adler_a = 1, adler_b = 0;
    const uint8_t *pixels = VirtualUncachedAddr(display->buffer);   /* what the graphics chip wrote, not old cache contents */
    for (int y = 0; y < height; y++) {
        line[0] = (y == height - 1) ? 1 : 0;    /* the last block says so */
        line[1] = row_bytes & 0xFF;
        line[2] = row_bytes >> 8;
        line[3] = ~row_bytes & 0xFF;
        line[4] = (~row_bytes >> 8) & 0xFF;
        uint8_t *out = &line[5];
        *out++ = 0;     /* no filter */
        const uint16_t *source = (const uint16_t *) (pixels + (y * display->stride));
        for (int x = 0; x < width; x++) {
            uint16_t p = source[x];     /* 5 bits each of red, green, blue, then 1 of alpha */
            uint8_t r = (p >> 11) & 31, g = (p >> 6) & 31, b = (p >> 1) & 31;
            *out++ = (r << 3) | (r >> 2);
            *out++ = (g << 3) | (g >> 2);
            *out++ = (b << 3) | (b >> 2);
        }
        for (uint32_t i = 0; i < row_bytes; i++) {
            adler_a = (adler_a + line[5 + i]) % 65521;
            adler_b = (adler_b + adler_a) % 65521;
        }
        fwrite(line, 1, 5 + row_bytes, f);
        crc = crc_add(crc, line, 5 + row_bytes);
    }
    uint8_t adler[4];
    put32(adler, (adler_b << 16) | adler_a);
    fwrite(adler, 1, 4, f);
    crc = crc_add(crc, adler, 4);
    put32(head, crc ^ 0xFFFFFFFFu);
    fwrite(head, 1, 4, f);

    chunk(f, "IEND", NULL, 0);
    bool ok = (ferror(f) == 0);
    ok = (fclose(f) == 0) && ok;
    return ok;
}

bool screenshot_button (joypad_port_t port, joypad_buttons_t pressed) {
    if (port != JOYPAD_PORT_2 || !pressed.start || !features_enabled(FEATURE_SCREENSHOTS)) {
        return false;
    }
    wanted = true;
    return true;
}

void screenshot_after_frame (surface_t *display) {
    if (!wanted || !display) {
        return;
    }
    wanted = false;
    uint64_t began = get_ticks_ms();

    if (!directory_exists(FOLDER) && !directory_create(FOLDER)) {
        debugf("screenshot: could not make %s\n", FOLDER);
        sound_play_effect(SFX_ERROR);
        return;
    }
    char path[64];
    do {
        snprintf(path, sizeof(path), FOLDER "/shot%04d.png", ++last_number);
    } while (file_exists(path) && last_number < 9999);

    if (!write_png(TEMP_PATH, display) || !safe_file_replace(TEMP_PATH, path)) {
        debugf("screenshot: could not save %s\n", path);
        sound_play_effect(SFX_ERROR);
        return;
    }
    debugf("screenshot: saved %s in %d ms\n", path, (int) (get_ticks_ms() - began));
    sound_play_effect(SFX_SETTING);
}
