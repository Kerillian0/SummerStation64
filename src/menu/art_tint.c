/**
 * @file art_tint.c
 * @brief Colors the ring round a cover to match the cover.
 */

#include "art_tint.h"
#include "menu_options.h"

/* Hues are sorted into this many slices of the color wheel. */
#define SLICES          (12)
/* Leave out the right-hand part of the box: the red N64 strip on US boxes. */
#define STRIP_PERCENT   (22)
/* A pixel counts only if it is this colorful and this bright (0-255). */
#define MIN_CHROMA      (40)
#define MIN_BRIGHT      (60)

static const char *names[RING_TINT_COUNT] = { "Off", "Game Info", "Everywhere" };

const char *art_tint_name (int choice) {
    return (choice >= 0 && choice < RING_TINT_COUNT) ? names[choice] : "";
}

bool art_tint_on_game_info (void) {
    return options_get(OPTION_RING_TINT) != RING_TINT_OFF;
}

bool art_tint_on_covers (void) {
    return options_get(OPTION_RING_TINT) == RING_TINT_EVERYWHERE;
}

bool art_tint_from (const surface_t *image, color_t *out) {
    if (!image || !image->buffer || surface_get_format(image) != FMT_RGBA16) {
        return false;
    }

    /* For each slice: how much of the picture is that color (each pixel
       weighted by how colorful it is), and the sum of those pixels. */
    uint32_t weight[SLICES] = { 0 };
    uint32_t sum_r[SLICES] = { 0 }, sum_g[SLICES] = { 0 }, sum_b[SLICES] = { 0 };

    int width = image->width * (100 - STRIP_PERCENT) / 100;
    for (int y = 0; y < image->height; y += 2) {
        const uint16_t *row = (const uint16_t *) ((const uint8_t *) image->buffer + y * image->stride);
        for (int x = 0; x < width; x += 2) {
            uint16_t p = row[x];
            int r = ((p >> 11) & 0x1F) << 3;
            int g = ((p >> 6) & 0x1F) << 3;
            int b = ((p >> 1) & 0x1F) << 3;
            int high = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
            int low = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
            int chroma = high - low;
            if (chroma < MIN_CHROMA || high < MIN_BRIGHT) {
                continue; /* grey, white or nearly black */
            }
            /* Where on the color wheel, 0 to 6 (red, yellow, green, cyan, blue, magenta). */
            float hue;
            if (high == r) {
                hue = (float) (g - b) / chroma;
                if (hue < 0.0f) hue += 6.0f;
            } else if (high == g) {
                hue = (float) (b - r) / chroma + 2.0f;
            } else {
                hue = (float) (r - g) / chroma + 4.0f;
            }
            int slice = ((int) (hue * SLICES / 6.0f)) % SLICES;
            weight[slice] += chroma;
            sum_r[slice] += r * chroma;
            sum_g[slice] += g * chroma;
            sum_b[slice] += b * chroma;
        }
    }

    /* The slice with the most color, counting half of each neighbour so a
       color that straddles two slices is not split in two. */
    int best = -1;
    uint32_t best_score = 0;
    for (int i = 0; i < SLICES; i++) {
        uint32_t score = weight[i] + (weight[(i + SLICES - 1) % SLICES] + weight[(i + 1) % SLICES]) / 2;
        if (weight[i] > 0 && score > best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best < 0) {
        return false;
    }

    int r = sum_r[best] / weight[best];
    int g = sum_g[best] / weight[best];
    int b = sum_b[best] / weight[best];

    /* Made vivid: the strongest channel full, the weakest down to a third
       of where it was, the same hue. */
    int high = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    int low = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    if (high <= low) {
        return false;
    }
    int floor = low / 3;
    int c[3] = { r, g, b };
    for (int i = 0; i < 3; i++) {
        c[i] = floor + ((c[i] - low) * (255 - floor)) / (high - low);
    }
    *out = RGBA32(c[0], c[1], c[2], 0xFF);
    return true;
}
