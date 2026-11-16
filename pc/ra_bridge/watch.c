/**
 * @file watch.c
 * @brief Checks a game's achievements against every frame (see watch.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook_ring.h"
#include "watch.h"
#include "rcheevos/include/rc_runtime.h"

#define RDRAM_END   (0x00800000UL)

typedef struct {
    uint32_t id;
    uint32_t points;
    char title[128];
    char description[256];
    bool met;
} achievement_t;

static rc_runtime_t runtime;
static bool runtime_ready = false;
static watch_view_t view;
static achievement_t *achievements = NULL;
static int achievement_count = 0;
static int met = 0;
static int skipped_pointers = 0;

/* The hook's list as planned: native word-aligned ranges, and where each sits in a frame's copy. */
static hook_range_t plan[HOOK_MAX_RANGES];
static uint32_t plan_offset[HOOK_MAX_RANGES];
static int plan_count = 0;
static int plan_bytes = 0;

/* The words the runtime read in the frame being checked (native, word-aligned). */
static uint32_t *words = NULL;
static int word_count = 0, word_room = 0;

static const uint8_t *frame_data = NULL;
static uint32_t frame_length = 0;
static uint32_t frame_now = 0;
static bool asking = false;         /* the first run: only noting what is read */
static bool missed = false;
static uint32_t misses = 0;

static void note_word (uint32_t word) {
    if (word_count > 0 && words[word_count - 1] == word) {
        return;
    }
    if (word_count == word_room) {
        int room = word_room ? word_room * 2 : 1024;
        uint32_t *bigger = realloc(words, room * sizeof(uint32_t));
        if (!bigger) {
            return;
        }
        words = bigger;
        word_room = room;
    }
    words[word_count++] = word;
}

static bool byte_at (uint32_t native, uint8_t *out) {
    int low = 0, high = plan_count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (native < plan[mid].address) {
            high = mid - 1;
        } else if (native >= plan[mid].address + plan[mid].length) {
            low = mid + 1;
        } else {
            uint32_t offset = plan_offset[mid] + (native - plan[mid].address);
            if (offset >= frame_length) {
                return false;
            }
            *out = frame_data[offset];
            return true;
        }
    }
    return false;
}

static uint32_t RC_CCONV read_memory (uint32_t address, uint8_t *buffer, uint32_t num_bytes, void *ud) {
    (void) ud;
    for (uint32_t i = 0; i < num_bytes; i++) {
        uint32_t a = address + i;
        buffer[i] = 0;
        if (a >= RDRAM_END) {
            continue;
        }
        uint32_t native = (view == WATCH_VIEW_SWAPPED) ? (a ^ 3) : a;
        note_word(native & ~3UL);
        if (!asking && !byte_at(native, &buffer[i])) {
            missed = true;
        }
    }
    return num_bytes;
}

static int by_value (const void *a, const void *b) {
    uint32_t x = *(const uint32_t *) a, y = *(const uint32_t *) b;
    return (x > y) - (x < y);
}

/* Pack the noted words into the hook's list: merge neighbours, widening the
   allowed gap until it fits in HOOK_MAX_RANGES ranges. */
static bool plan_from_words (void) {
    hook_range_t before[HOOK_MAX_RANGES];
    int before_count = plan_count;
    memcpy(before, plan, sizeof(before));
    qsort(words, word_count, sizeof(uint32_t), by_value);
    int unique = 0;
    for (int i = 0; i < word_count; i++) {
        if (unique == 0 || words[unique - 1] != words[i]) {
            words[unique++] = words[i];
        }
    }
    word_count = unique;
    for (uint32_t gap = 16; ; gap *= 2) {
        int count = 0, bytes = 0;
        for (int i = 0; i < word_count && count <= HOOK_MAX_RANGES; ) {
            uint32_t start = words[i], end = words[i] + 4;
            while (i + 1 < word_count && words[i + 1] <= end + gap) {
                end = words[++i] + 4;
            }
            i++;
            if (count < HOOK_MAX_RANGES) {
                plan[count].address = start;
                plan[count].length = end - start;
                plan_offset[count] = bytes;
            }
            bytes += end - start;
            count++;
        }
        if (count <= HOOK_MAX_RANGES || gap > RDRAM_END) {
            plan_count = (count > HOOK_MAX_RANGES) ? HOOK_MAX_RANGES : count;
            plan_bytes = bytes;
            if (bytes > HOOK_MAX_BYTES) {
                printf("\nNote: the achievements read %d bytes a frame, more than the hook's %d: some won't be checked.\n", bytes, HOOK_MAX_BYTES);
            }
            break;
        }
    }
    if (plan_count == before_count && memcmp(plan, before, plan_count * sizeof(hook_range_t)) == 0) {
        return true;    /* the same list (e.g. more than fits): nothing to send */
    }
    return hook_set_list(plan, plan_count);
}

static achievement_t *find (uint32_t id) {
    for (int i = 0; i < achievement_count; i++) {
        if (achievements[i].id == id) {
            return &achievements[i];
        }
    }
    return NULL;
}

static void RC_CCONV on_event (const rc_runtime_event_t *event) {
    if (event->type != RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED || asking) {
        return;
    }
    achievement_t *a = find(event->id);
    if (a && !a->met) {
        a->met = true;
        met++;
        printf("\rframe %8lu: WOULD UNLOCK  %s (%lu points): %s%20s\n", (unsigned long) frame_now, a->title,
               (unsigned long) a->points, a->description, "");
    }
}

int watch_load (const rc_api_fetch_game_data_response_t *game, watch_view_t which) {
    view = which;
    if (runtime_ready) {
        rc_runtime_destroy(&runtime);
    }
    rc_runtime_init(&runtime);
    runtime_ready = true;
    free(achievements);
    achievements = calloc(game->num_achievements ? game->num_achievements : 1, sizeof(achievement_t));
    achievement_count = 0;
    met = 0;
    skipped_pointers = 0;
    for (uint32_t i = 0; achievements && i < game->num_achievements; i++) {
        const rc_api_achievement_definition_t *d = &game->achievements[i];
        if (d->category != RC_ACHIEVEMENT_CATEGORY_PROMOTED || !d->definition) {
            continue;
        }
        if (d->id == 101000001UL) {
            continue;   /* the server's own "Warning: Unknown Emulator" for clients it doesn't know (1=1.300) */
        }
        /* Achievements that follow pointers ("I:", AddAddress) need memory that moves from frame to
           frame, more than the hook can carry: not checked yet, rather than checked wrongly. */
        if (strstr(d->definition, "I:")) {
            skipped_pointers++;
            continue;
        }
        if (rc_runtime_activate_achievement(&runtime, d->id, d->definition, NULL, 0) != RC_OK) {
            printf("Could not read achievement %lu (%s); skipped.\n", (unsigned long) d->id, d->title ? d->title : "");
            continue;
        }
        achievement_t *a = &achievements[achievement_count++];
        a->id = d->id;
        a->points = d->points;
        snprintf(a->title, sizeof(a->title), "%s", d->title ? d->title : "");
        snprintf(a->description, sizeof(a->description), "%s", d->description ? d->description : "");
    }
    return achievement_count;
}

bool watch_plan (void) {
    word_count = 0;
    asking = true;
    rc_runtime_do_frame(&runtime, on_event, read_memory, NULL, NULL);
    asking = false;
    rc_runtime_reset(&runtime);     /* that frame saw only zeros: start the achievements afresh */
    return plan_from_words();
}

void watch_frame (uint32_t frame, const uint8_t *data, uint32_t length, void *context) {
    (void) context;
    frame_data = data;
    frame_length = length;
    frame_now = frame;
    word_count = 0;
    missed = false;
    rc_runtime_do_frame(&runtime, on_event, read_memory, NULL, NULL);
    if (missed) {
        misses++;
        plan_from_words();      /* this frame read everything the achievements need now */
    }
}

uint32_t watch_misses (void) {
    return misses;
}

int watch_skipped (void) {
    return skipped_pointers;
}

int watch_met (void) {
    return met;
}

void watch_list_size (int *ranges, int *bytes) {
    *ranges = plan_count;
    *bytes = plan_bytes;
}
