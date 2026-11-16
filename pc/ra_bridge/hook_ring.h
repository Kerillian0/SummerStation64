/**
 * @file hook_ring.h
 * @brief The game-side hook's protocol (src/boot/ra_hook.h), from the PC:
 * give it its list of entries (copy a range, or follow a pointer), then take
 * every frame's copy from its ring of 16, in order and once each.
 */

#ifndef HOOK_RING_H__
#define HOOK_RING_H__

#include <stdbool.h>
#include <stdint.h>

#define HOOK_MAX_ENTRIES    (255)
#define HOOK_MAX_BYTES      (4092)      /* a slot's room for one frame's copy */
#define HOOK_POINTER        (0x80000000UL)  /* follow a pointer: base = low 24 bits of the word at base + address */
#define HOOK_LAST           (0x40000000UL)  /* ... last frame's pointer instead (kept in `length`, start it at 0) */
#define HOOK_TELL           (0x40000000UL)  /* copy: the RDRAM address copied from goes first (4 bytes) */

/** One entry of the hook's list (see ra_hook.h). */
typedef struct {
    uint32_t address;   /**< RDRAM offset added to the base, 0x00000000-0x007FFFFF, plus the flags above */
    uint32_t length;    /**< copy: bytes, a multiple of 4 */
} hook_entry_t;

/** Called for each new frame, oldest first: `data` holds the ranges' bytes back to back. */
typedef void (*hook_frame_fn) (uint32_t frame, const uint8_t *data, uint32_t length, void *context);

/** Give the hook its list (it starts copying from the next frame). */
bool hook_set_list (const hook_entry_t *entries, int count);

/**
 * Read the ring and hand every frame not seen yet to `fn`, in order.
 * @param running  Out: the hook has started (its status says "RAH3").
 * @return false if the cart could not be read.
 */
bool hook_poll (hook_frame_fn fn, void *context, bool *running);

/** The game's hook is an older one (the menu on the card is older than this program). */
bool hook_is_old (void);

/** Frames handed on, and frames lost (the ring had moved past them), since the start. */
uint32_t hook_frames_seen (void);
uint32_t hook_frames_missed (void);

#endif
