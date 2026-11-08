/**
 * @file video_clip_check.h
 * @brief A look at the start of a preview clip before it is played.
 *
 * libdragon's H.264 player stops the console with an error screen on a
 * stream it can't play, so a clip is only handed to it if its first
 * parameters look like one made by scripts/make_preview_clip.py: an H.264
 * stream starting with its sequence header, a picture size the menu can
 * draw (a width in steps of 32, as the picture blitter needs) and no larger
 * than the TV picture.
 *
 * Plain C with no libdragon, so it is tested on the PC (tests/video_clip_test.c).
 */

#ifndef VIDEO_CLIP_CHECK_H__
#define VIDEO_CLIP_CHECK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VIDEO_CLIP_MAX_WIDTH    (320)
#define VIDEO_CLIP_MAX_HEIGHT   (240)

typedef enum {
    VIDEO_CLIP_OK,
    VIDEO_CLIP_NOT_H264,        /**< doesn't start with an H.264 sequence header */
    VIDEO_CLIP_BAD_HEADER,      /**< the header ends early or holds values out of range */
    VIDEO_CLIP_UNSUPPORTED,     /**< a kind of stream the player doesn't take (interlaced, 4:2:2, ...) */
    VIDEO_CLIP_BAD_SIZE,        /**< too big, too small, or a width that isn't a multiple of 32 */
} video_clip_result_t;

typedef struct {
    int width;      /**< pixels */
    int height;
} video_clip_info_t;

/**
 * Check the first bytes of a clip (a few hundred are plenty; the sequence
 * header comes first). @p info may be NULL.
 */
video_clip_result_t video_clip_check (const uint8_t *data, size_t size, video_clip_info_t *info);

/** How many pictures a whole clip holds (the player uses it to loop smoothly). */
int video_clip_count_frames (const uint8_t *data, size_t size);

#endif /* VIDEO_CLIP_CHECK_H__ */
