/**
 * @file hook_ring.h
 * @brief The game-side hook's protocol (src/boot/ra_hook.h), from the PC:
 * tell it which parts of the game's memory to copy, then take every frame's
 * copy from its ring of 16, in order and once each.
 */

#ifndef HOOK_RING_H__
#define HOOK_RING_H__

#include <stdbool.h>
#include <stdint.h>

#define HOOK_MAX_RANGES     (31)
#define HOOK_MAX_BYTES      (4092)      /* a slot's room for one frame's copy */

typedef struct {
    uint32_t address;   /**< RDRAM offset, 0x00000000-0x007FFFFF */
    uint32_t length;    /**< bytes, a multiple of 4 */
} hook_range_t;

/** Called for each new frame, oldest first: `data` holds the ranges' bytes back to back. */
typedef void (*hook_frame_fn) (uint32_t frame, const uint8_t *data, uint32_t length, void *context);

/** Give the hook its list (it starts copying from the next frame). */
bool hook_set_list (const hook_range_t *ranges, int count);

/**
 * Read the ring and hand every frame not seen yet to `fn`, in order.
 * @param running  Out: the hook has started (its status says "RAH2").
 * @return false if the cart could not be read.
 */
bool hook_poll (hook_frame_fn fn, void *context, bool *running);

/** Frames handed on, and frames lost (the ring had moved past them), since the start. */
uint32_t hook_frames_seen (void);
uint32_t hook_frames_missed (void);

#endif
