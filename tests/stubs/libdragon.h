/* Stand-in for libdragon.h, so code that only needs its color type can be
   built and tested on the PC. Only what the tested files use is here. */
#ifndef TEST_STUB_LIBDRAGON_H__
#define TEST_STUB_LIBDRAGON_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } color_t;
#define RGBA32(rx, gx, bx, ax) ((color_t) { .r = (rx), .g = (gx), .b = (bx), .a = (ax) })

typedef struct { void *buffer; uint16_t width, height, stride; } surface_t;

#endif
