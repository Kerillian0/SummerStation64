/**
 * @file hook_ring.c
 * @brief The game-side hook's protocol, from the PC (see hook_ring.h).
 */

#include <stdlib.h>
#include <string.h>

#include "hook_ring.h"
#include "sc64_link.h"

/* src/boot/ra_hook.h, as the cart's own (USB) addresses */
#define LIST        (0x03B00000UL)
#define STATUS      (0x03B01000UL)
#define RING        (0x03B10000UL)
#define SLOTS       (16)
#define SLOT_SIZE   (4096)

static uint32_t copy_size = 0;
static hook_range_t list_kept[HOOK_MAX_RANGES];    /* sent again if the menu clears it (each launch does) */
static int list_count = 0;
static bool have_last = false;
static uint32_t last_frame = 0;
static uint32_t seen = 0, missed = 0;
static uint8_t ring[SLOTS * SLOT_SIZE];

static void put32 (uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t get32 (const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

bool hook_set_list (const hook_range_t *ranges, int count) {
    if (count > HOOK_MAX_RANGES) {
        count = HOOK_MAX_RANGES;
    }
    if (ranges != list_kept) {
        memcpy(list_kept, ranges, count * sizeof(hook_range_t));
        list_count = count;
    }
    uint8_t list[8 + HOOK_MAX_RANGES * 8];
    put32(list, 0);         /* not valid while it changes ... */
    put32(list + 4, (uint32_t) count);
    copy_size = 0;
    for (int i = 0; i < count; i++) {
        put32(list + 8 + i * 8, ranges[i].address);
        put32(list + 12 + i * 8, ranges[i].length);
        copy_size += ranges[i].length;
    }
    if (copy_size > HOOK_MAX_BYTES) {
        copy_size = HOOK_MAX_BYTES;
    }
    if (!sc64_write(LIST, list, 8 + count * 8)) {
        return false;
    }
    return sc64_write(LIST, "RAL1", 4);   /* ... until "RAL1" goes in, last */
}

typedef struct { uint32_t frame; int slot; } found_t;

static int by_frame (const void *a, const void *b) {
    uint32_t x = ((const found_t *) a)->frame, y = ((const found_t *) b)->frame;
    return (x > y) - (x < y);
}

bool hook_poll (hook_frame_fn fn, void *context, bool *running) {
    uint8_t status[8];
    if (!sc64_read(STATUS, status, sizeof(status))) {
        return false;
    }
    *running = (memcmp(status, "RAH2", 4) == 0);
    if (!*running) {
        return true;
    }
    uint32_t now = get32(status + 4);
    uint8_t list_magic[4];
    if (!sc64_read(LIST, list_magic, sizeof(list_magic))) {
        return false;
    }
    if (memcmp(list_magic, "RAL1", 4) != 0 && list_count > 0) {
        return hook_set_list(list_kept, list_count);  /* a new launch cleared it: the next frames will have it */
    }
    if (!sc64_read(RING, ring, sizeof(ring))) {
        return false;
    }
    found_t found[SLOTS];
    int count = 0;
    for (int slot = 0; slot < SLOTS; slot++) {
        uint32_t frame = get32(ring + slot * SLOT_SIZE);
        /* Only frames the hook can't have started overwriting while we read. */
        if (frame + (SLOTS - 2) >= now && frame <= now && (int) (frame % SLOTS) == slot
                && (!have_last || frame > last_frame)) {
            found[count].frame = frame;
            found[count].slot = slot;
            count++;
        }
    }
    qsort(found, count, sizeof(found[0]), by_frame);
    for (int i = 0; i < count; i++) {
        if (have_last && found[i].frame > last_frame + 1) {
            missed += found[i].frame - last_frame - 1;
        }
        fn(found[i].frame, ring + found[i].slot * SLOT_SIZE + 4, copy_size, context);
        last_frame = found[i].frame;
        have_last = true;
        seen++;
    }
    return true;
}

uint32_t hook_frames_seen (void) {
    return seen;
}

uint32_t hook_frames_missed (void) {
    return missed;
}
