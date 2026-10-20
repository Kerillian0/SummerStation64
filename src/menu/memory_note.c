#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <libdragon.h>

#include "memory_note.h"

static bool cut[MEMORY_CUT_COUNT];

static const char *what_was_cut[MEMORY_CUT_COUNT] = {
    [MEMORY_CUT_SIDE_ART] = "art on the side covers",
    [MEMORY_CUT_BAKED_ART] = "covers from the menu file",
    [MEMORY_CUT_INTRO_PICTURE] = "the intro's title picture",
};

static int free_kb (void) {
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    return (heap.total - heap.used) / 1024;
}

void memory_note_cut (memory_cut_t what) {
    if (what < 0 || what >= MEMORY_CUT_COUNT || cut[what]) {
        return;
    }
    cut[what] = true;
    debugf("memory: short, left out %s (%d KB free)\n", what_was_cut[what], free_kb());
}

const char *memory_note_value (void) {
    static char text[24];
    snprintf(text, sizeof(text), "%d KB free", free_kb());
    return text;
}

const char *memory_note_details (void) {
    static char text[160];
    int used = snprintf(text, sizeof(text), "\nLeft out to save memory: ");
    int count = 0;
    for (int i = 0; i < MEMORY_CUT_COUNT; i++) {
        if (cut[i] && used < (int) sizeof(text)) {
            used += snprintf(text + used, sizeof(text) - used, "%s%s", count ? ", " : "", what_was_cut[i]);
            count++;
        }
    }
    if (count == 0) {
        return "\nNothing has been left out.";
    }
    if (used < (int) sizeof(text)) {
        snprintf(text + used, sizeof(text) - used, ".");
    }
    return text;
}
