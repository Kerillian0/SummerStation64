/**
 * @file art_trim.h
 * @brief Leaves out the black frame some box art scans carry.
 *
 * About half the covers in the metadata pack have a near-black border of
 * 2-4 pixels (a few up to 7), which showed as dark lines down the edges of
 * the cover and the 3D box; every top label in the pack has a dark line at
 * one end. When a box picture (front, back, top or side) has been unpacked
 * from the card, any edge row or column that is near-black almost all the way
 * across is left out, at most a twentieth of the picture on each side, so
 * covers black by design (the PAL Ocarina of Time box) keep their shape.
 * The picture is moved up in its own memory: nothing is copied or added.
 * The bake script (assets/boxart/make_baked_art.py) trims baked art the
 * same way. Cartridges are left alone. Feature `trim_edges`.
 */

#ifndef ART_TRIM_H__
#define ART_TRIM_H__

#include <libdragon.h>

#include "png_decoder.h"
#include "ui_components.h"

/** Say which picture the next decode is (the decoder unpacks one at a time). */
void art_trim_next (file_image_type_t which);

/** Trim a freshly unpacked picture, if it is a box front or back and the feature is on. */
void art_trim_decoded (png_err_t err, surface_t *image);

#endif /* ART_TRIM_H__ */
