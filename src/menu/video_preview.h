/**
 * @file video_preview.h
 * @brief A short looping clip of the game in the cover's place on Game info.
 *
 * A clip is an H.264 stream made by scripts/make_preview_clip.py and kept
 * beside the game's box art on the SD card:
 *
 *     menu/metadata/N/S/M/E/preview.h264   (or N/S/M/preview.h264 for every region)
 *     menu/metadata/N/S/M/E/preview.seek   (its seek table, for looping without a pause)
 *
 * How it stays out of the menu's way:
 *   - The whole clip (a few hundred KB) is read into memory in 64 KB steps,
 *     one per frame, so the SD card is never read while it plays and a loop
 *     back to the start costs nothing.
 *   - libdragon's H.264 decoder does the heavy work on the RSP. The CPU part
 *     runs in small slices (32 macroblocks), mostly in the time the menu
 *     would otherwise spend waiting for the next frame (video_preview_poll),
 *     and keeps a few pictures decoded ahead so a slow moment doesn't show.
 *   - The clip keeps its own clock: if decoding ever falls behind, the clip
 *     slows down for a moment instead of the menu dropping frames.
 *
 * Memory, only while Game info is open with a clip: the clip itself (at
 * most 3 MB; a 192x144 clip of 25 seconds is 150 to 400 KB), the decoder's
 * pictures (BUFFERED_PICS of about 42 KB at 192x144, plus its reference
 * pictures) and one held picture: about 1.2 MB in all for the usual clip,
 * measured in ares. Everything is freed when the screen closes or a game
 * starts. Hence Expansion Pak only (and the Video Previews switch in
 * Settings, the same for every profile). Before loading the clip and
 * before starting the decoder, it checks there is room for them (the decoder
 * stops the console if it runs out); if not, the cover stays and Settings >
 * System > Memory lists the clip as left out.
 *
 * A clip the player can't take (a bad header, too big) leaves the cover
 * showing; so does a game with no clip, and a hack (load_rom.c).
 */

#ifndef VIDEO_PREVIEW_H__
#define VIDEO_PREVIEW_H__

#include <stdbool.h>
#include <libdragon.h>
#include <stddef.h>
#include <stdint.h>

/** Name of a clip file in a game's metadata folder. */
#define VIDEO_PREVIEW_FILE  "preview.h264"

/**
 * Turn previews on or off for the whole console (Settings > Video Previews,
 * saved in config.ini). Off stops a clip that is playing. They are also off
 * without the Expansion Pak, whatever this says.
 */
void video_preview_enable (bool on);

/**
 * The clip for this game code on the SD card, if there is one: the full
 * path is written to @p out. Looks in the region's folder first, then in the
 * folder for all regions, as box art does. Reads the card; call once per game.
 * @param storage_prefix  The card's prefix (menu->storage_prefix, "sd:/").
 */
bool video_preview_find (const char *storage_prefix, const char game_code[4], char *out, size_t size);

/**
 * Call every frame the clip could be shown, with the clip's full path. A
 * different path from last time starts over with the new clip. The clip
 * appears after a short wait, so the cover is seen first.
 */
void video_preview_update (const char *path);

/**
 * Draw the clip's current picture to fit the box (keeping its shape,
 * centred). Returns false, drawing nothing, until a picture is ready.
 */
bool video_preview_draw (int x, int y, int w, int h);

/**
 * Draw the clip's current picture into `target` (an RGBA16 picture), its
 * middle cropped to the target's shape and stretched to fill it, then
 * `over` (the cover) on top at `over_alpha` (0 to 1) if given: the front of
 * a 3D box with the clip on it. Returns false, drawing nothing, until a
 * picture is ready.
 */
bool video_preview_render (surface_t *target, const surface_t *over, float over_alpha);

/** The size the picture would be drawn at in a box this size, if the clip is ready. */
bool video_preview_fit (int w, int h, int *shown_w, int *shown_h);

/** 0 to 1 while the clip fades in over the cover; 1 once it has. */
float video_preview_fade (void);

/**
 * A screen opening with the clip that is playing (Game info, from its
 * cover): the next video_preview_update() with the same path carries on
 * where it is, without fading in again. Does nothing for another clip. A
 * clip no screen draws for 4 seconds is stopped and freed by
 * video_preview_poll().
 */
void video_preview_carry_on (const char *path);

/** Whether this clip is playing now with a picture to show (carried on from a cover). */
bool video_preview_playing (const char *path);

/**
 * Draw the clip's picture stretched into x, y, w, h, for a box that moves or
 * changes size every frame (Game info's flight): drawn into a picture of
 * the clip's own size first, so nothing is recorded again per frame.
 */
bool video_preview_draw_moving (int x, int y, int w, int h);

/** Decode a little ahead. Call while the menu waits for the next frame. */
void video_preview_poll (void);

/** Stop and free everything (leaving the screen, starting a game). */
void video_preview_stop (void);

/** How the clip being played is getting on (for tests and the Performance Overlay). */
typedef struct {
    int width, height;
    float fps;
    uint32_t frames_shown;      /**< pictures put on screen since it started */
    uint32_t late;              /**< frames when the clip had to wait for the decoder */
    uint32_t loops;             /**< times it went round to the start */
    uint32_t idle_slices;       /**< decoding slices done in the menu's idle time */
    uint32_t update_us;         /**< the last video_preview_update, microseconds */
    uint32_t update_us_max;     /**< the longest while playing */
    uint32_t loop_us_max;       /**< the longest going round to the start at once (when it can't go smoothly) */
    uint32_t next_us_max;       /**< the longest wait for the next picture (0-ish when decoded ahead) */
} video_preview_stats_t;

const video_preview_stats_t *video_preview_stats (void);

/** A clip is playing and being shown (the menu paces itself to it: frame_rate.h). */
bool video_preview_showing (void);

#endif /* VIDEO_PREVIEW_H__ */
