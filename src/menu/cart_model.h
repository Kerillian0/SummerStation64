/**
 * @file cart_model.h
 * @brief One game's cartridge as a 3D model, for the animations that show
 * it on its own: starting a game (game_start.c) and coming back from one
 * (cover_deal.c).
 *
 * Its front is the game's baked cartridge picture, or the plain grey
 * cartridge for a game without one; its back is the back every cartridge
 * shares, the plastic coloured to match the front; its bottom shows the
 * circuit board. Everything is read into memory when it is loaded (the
 * game start needs that: loading a game overwrites rom:/). One at a time.
 */

#ifndef CART_MODEL_H__
#define CART_MODEL_H__

#include <stdbool.h>
#include "box3d.h"

/** Load the cartridge for game code `code` (NULL, or none baked: the plain one). Frees the one before. */
void cart_model_load (const char *code);

/** Let it go. */
void cart_model_free (void);

/** There is a picture to draw (false only when memory was short). */
bool cart_model_ready (void);

/** It has the game's own picture (not the plain one). */
bool cart_model_own (void);

/** Its height against its width. */
float cart_model_shape (void);

/**
 * Draw it, turned `turn` degrees round its upright (past a quarter turn its
 * back shows), at `pose` (fitted into x0, y0, w, h, or in the shared space
 * with pose->world); `light` 0-255 brightness, `alpha` 255 solid.
 */
void cart_model_draw (const box3d_pose_t *pose, int x0, int y0, int w, int h, float turn, int light, int alpha);

#endif
