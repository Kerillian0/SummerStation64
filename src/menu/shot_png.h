/**
 * @file shot_png.h
 * @brief Writes a PNG picture a line at a time, compressed, in little memory.
 *
 * Used by screenshots. The lines are asked for one by one, so the picture
 * never has to be held whole. The compression looks for repeats only in a
 * few places (one, two and four pixels back, and the lines above), which is
 * quick and suits the menu's flat areas and dither patterns. Needs no
 * libdragon, so it is tested on the PC (tests/shot_png_test.c).
 */

#ifndef SHOT_PNG_H__
#define SHOT_PNG_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define SHOT_PNG_MAX_WIDTH  (640)

/** Fills `rgb` with line `y`: red, green and blue for each pixel. */
typedef void (*shot_png_line_t) (int y, uint8_t *rgb, void *context);

/**
 * Write a width x height picture to `f`.
 * Takes about 14 KB of memory while it works (allocated, then freed).
 * @return false if it could not (no memory, too wide, or a write failed).
 */
bool shot_png_write (FILE *f, int width, int height, shot_png_line_t line, void *context);

#endif /* SHOT_PNG_H__ */
