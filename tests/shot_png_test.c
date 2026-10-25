// PC-side test for the screenshot PNG writer (src/menu/shot_png.c): writes
// pictures of several kinds, with the raw pixels beside each, for
// tests/shot_png_check.py to unpack with zlib and compare. Run with tests/run.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shot_png.h"

typedef struct { int kind, width, height; FILE *raw; } picture_t;

static uint32_t seed = 12345;
static uint8_t noise (void) {
    seed = seed * 1103515245u + 12345u;
    return seed >> 24;
}

static void line (int y, uint8_t *rgb, void *context) {
    picture_t *p = context;
    for (int x = 0; x < p->width; x++) {
        uint8_t *px = &rgb[x * 3];
        switch (p->kind) {
            case 0:     // flat
                px[0] = 20; px[1] = 30; px[2] = 60;
                break;
            case 1:     // a 2x2 dither over a gradient, the menu's kind of picture
                px[0] = (y / 3) + ((x + y) & 1) * 8;
                px[1] = (x / 5) & 0xFF;
                px[2] = 90 + (((x & 1) ^ (y & 1)) ? 8 : 0);
                break;
            case 2:     // noise: nothing repeats
                px[0] = noise(); px[1] = noise(); px[2] = noise();
                break;
            default:    // long runs, then a change, lines alike with a shift
                px[0] = ((x + (y & 7)) / 40) * 30;
                px[1] = (x > 300 && y > 200) ? 255 : 0;
                px[2] = (x % 7 == 0) ? 200 : 10;
                break;
        }
    }
    fwrite(rgb, 1, p->width * 3, p->raw);
}

int main (int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    const int sizes[][2] = { { 640, 480 }, { 320, 240 }, { 1, 1 }, { 7, 3 } };
    int written = 0;
    for (int kind = 0; kind < 4; kind++) {
        for (int s = 0; s < 4; s++) {
            char png_path[512], raw_path[512];
            snprintf(png_path, sizeof(png_path), "%s/shot_%d_%dx%d.png", dir, kind, sizes[s][0], sizes[s][1]);
            snprintf(raw_path, sizeof(raw_path), "%s/shot_%d_%dx%d.raw", dir, kind, sizes[s][0], sizes[s][1]);
            picture_t p = { kind, sizes[s][0], sizes[s][1], fopen(raw_path, "wb") };
            FILE *f = fopen(png_path, "wb");
            if (!f || !p.raw || !shot_png_write(f, p.width, p.height, line, &p)) {
                printf("  FAIL: could not write %s\n", png_path);
                return 1;
            }
            fclose(f);
            fclose(p.raw);
            written++;
        }
    }
    if (shot_png_write(stdout, 641, 1, line, NULL)) {
        printf("  FAIL: a picture wider than 640 was written\n");
        return 1;
    }
    printf("%d pictures written\n", written);
    return 0;
}
