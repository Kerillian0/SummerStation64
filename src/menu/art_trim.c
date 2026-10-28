#include <string.h>

#include "art_trim.h"
#include "menu_features.h"

#define DARK_SUM        (96)    /* r + g + b below this (0-255 each) counts as near-black */
#define DARK_PERCENT    (97)    /* a line is part of the frame when this share of it is near-black */

static bool trim_next = false;

void art_trim_next (file_image_type_t which) {
    trim_next = (which == IMAGE_BOXART_FRONT || which == IMAGE_BOXART_BACK || which == IMAGE_BOXART_TOP
              || which == IMAGE_BOXART_BOTTOM || which == IMAGE_BOXART_LEFT || which == IMAGE_BOXART_RIGHT);
}

static bool dark (const surface_t *image, int x, int y) {
    const uint8_t *row = (const uint8_t *) image->buffer + y * image->stride;
    int sum;
    if (surface_get_format(image) == FMT_RGBA32) {
        const uint8_t *p = row + x * 4;
        sum = p[0] + p[1] + p[2];
    } else {
        uint16_t p = ((const uint16_t *) row)[x];
        sum = (((p >> 11) & 31) + ((p >> 6) & 31) + ((p >> 1) & 31)) * 255 / 31;
    }
    return sum < DARK_SUM;
}

static bool dark_row (const surface_t *image, int y) {
    int n = 0;
    for (int x = 0; x < image->width; x++) n += dark(image, x, y);
    return n * 100 >= DARK_PERCENT * image->width;
}

static bool dark_column (const surface_t *image, int x) {
    int n = 0;
    for (int y = 0; y < image->height; y++) n += dark(image, x, y);
    return n * 100 >= DARK_PERCENT * image->height;
}

static int most (int size) {
    return size / 20 > 2 ? size / 20 : 2;
}

void art_trim_decoded (png_err_t err, surface_t *image) {
    bool wanted = trim_next;
    trim_next = false;
    if (err != PNG_OK || !image || !wanted || !features_enabled(FEATURE_TRIM_EDGES)) {
        return;
    }
    tex_format_t format = surface_get_format(image);
    if ((format != FMT_RGBA16 && format != FMT_RGBA32) || image->width < 16 || image->height < 16) {
        return;
    }
    int w = image->width, h = image->height;
    int top = 0, bottom = 0, left = 0, right = 0;
    while (top < most(h) && dark_row(image, top)) top++;
    while (bottom < most(h) && dark_row(image, h - 1 - bottom)) bottom++;
    while (left < most(w) && dark_column(image, left)) left++;
    while (right < most(w) && dark_column(image, w - 1 - right)) right++;
    if (top + bottom + left + right == 0) {
        return;
    }
    /* Move the inner part to the top left of the same memory, row by row
       (each row moves to an earlier address, so none is overwritten before
       it is read), and keep the stride. */
    int bytes = (format == FMT_RGBA32) ? 4 : 2;
    int nw = w - left - right, nh = h - top - bottom;
    uint8_t *buffer = image->buffer;
    for (int y = 0; y < nh; y++) {
        memmove(buffer + y * image->stride, buffer + (y + top) * image->stride + left * bytes, nw * bytes);
    }
    image->width = nw;
    image->height = nh;
    data_cache_hit_writeback(buffer, image->stride * nh);
    debugf("art trim: %dx%d less %d top, %d bottom, %d left, %d right\n", w, h, top, bottom, left, right);
}
