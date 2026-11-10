/**
 * @file avatars.h
 * @brief Profile pictures: what there is to choose from, and drawing one.
 *
 * Pictures come from two places: those baked into the menu file
 * (rom:/avatars/, made by assets/profiles/make_avatars.py from pictures that
 * are never in the repository) and any PNG in sd:/menu/avatars/. A profile
 * keeps its choice as a short name: "rom:Mario" or "sd:Bowser.png"; "" is
 * no picture, drawn as the player's initial.
 *
 * Every picture is a 64 x 64 square (a card picture is shrunk as it is read
 * and cut square from its middle), cut to a circle or a rounded square
 * (option `avatar_shape`, the same for every player) when it is loaded, and
 * drawn in a ring of the player's color. Pictures are loaded when first
 * drawn, a card one a little later than a baked one, and kept until
 * avatars_forget_unused().
 */

#ifndef AVATARS_H__
#define AVATARS_H__

#include <stdbool.h>
#include <libdragon.h>

/** Option `avatar_shape` in options.ini. */
typedef enum {
    AVATAR_SHAPE_ROUND,     /**< the default */
    AVATAR_SHAPE_SQUARE,    /**< a rounded square */
    AVATAR_SHAPE_COUNT
} avatar_shape_t;

/** The name of a shape, for the settings screen. */
const char *avatars_shape_name (int shape);

/** Read the card's folder again (the baked list is read once). */
void avatars_rescan (void);

/** How many choices there are: "None" first, then the baked pictures, then the card's. */
int avatars_count (void);

/** A choice's short name, as profiles keep it ("" for None). */
const char *avatars_ref (int choice);

/** Which choice a short name is, or 0 (None) if it isn't there any more. */
int avatars_find (const char *ref);

/** What to call a picture: "None", "Mario", "Bowser" (a card file without its extension). */
const char *avatars_label (const char *ref);

/**
 * Draw a picture `size` pixels across at x, y, in a ring of `color`. With no
 * picture, or while it loads, the ring's shape is filled with the color and
 * the first letter of `name` is drawn on it.
 */
void avatars_draw (const char *ref, int x, int y, int size, color_t color, const char *name);

/** Let go of every loaded picture that no profile uses (after the choosing grid). */
void avatars_forget_unused (void);

/** The shape changed: let go of every picture, so each is cut again when next drawn. */
void avatars_shape_changed (void);

#endif /* AVATARS_H__ */
