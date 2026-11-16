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
/* rcheevos's own parsed form: which memory each achievement reads, and how pointers are followed. */
#include "rcheevos/src/runtime/rc_memref.h"
#include "rcheevos/src/runtime/rc_modified_memref.h"
#include "rcheevos/src/runtime/rc_operand.h"
#include "rcheevos/src/runtime/rc_operator.h"

#define RDRAM_END       (0x00800000UL)
#define MAX_RANGES      (96)        /* fixed ranges; the rest of the list follows pointers */
#define MAX_DEPTH       (8)

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

/* The hook's list: the fixed ranges first (native, word-aligned, sorted), then the pointer chains. */
static hook_entry_t list[HOOK_MAX_ENTRIES];
static int list_count = 0;
static int range_count = 0;
static uint32_t range_offset[MAX_RANGES];       /* where each fixed range sits in a frame's copy */
static int list_bytes = 0;

/* Each chain ends in a copy that says where it copied from: where that sits in a frame, and its length. */
static uint32_t chain_offset[HOOK_MAX_ENTRIES];
static uint32_t chain_length[HOOK_MAX_ENTRIES];
static uint32_t chain_address[HOOK_MAX_ENTRIES];    /* this frame's */
static int chain_count = 0;
static int chains_left_out = 0;

/* Words to copy, while planning (native, word-aligned). */
static uint32_t *words = NULL;
static int word_count = 0, word_room = 0;

static const uint8_t *frame_data = NULL;
static uint32_t frame_length = 0;
static uint32_t frame_now = 0;
static bool missed = false;
static uint32_t misses = 0;

static uint32_t get32 (const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

static void note_word (uint32_t word) {
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

/* The console's own address of byte `a` of RetroAchievements' view. */
static uint32_t native_of (uint32_t a) {
    return (view == WATCH_VIEW_SWAPPED) ? (a ^ 3) : a;
}

static bool byte_at (uint32_t native, uint8_t *out) {
    int low = 0, high = range_count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (native < list[mid].address) {
            high = mid - 1;
        } else if (native >= list[mid].address + list[mid].length) {
            low = mid + 1;
        } else {
            uint32_t offset = range_offset[mid] + (native - list[mid].address);
            if (offset >= frame_length) {
                return false;
            }
            *out = frame_data[offset];
            return true;
        }
    }
    for (int i = 0; i < chain_count; i++) {     /* where pointers led this frame */
        if (native >= chain_address[i] && native < chain_address[i] + chain_length[i]) {
            uint32_t offset = chain_offset[i] + 4 + (native - chain_address[i]);
            if (offset >= frame_length) {
                return false;
            }
            *out = frame_data[offset];
            return true;
        }
    }
    return false;
}

#ifdef WATCH_TEST
const uint8_t *watch_test_ram = NULL;   /* tests/ra_watch_test.c: the console's memory, to check each byte read */
uint32_t watch_test_reads = 0, watch_test_wrong = 0;
#endif

static uint32_t RC_CCONV read_memory (uint32_t address, uint8_t *buffer, uint32_t num_bytes, void *ud) {
    (void) ud;
    for (uint32_t i = 0; i < num_bytes; i++) {
        uint32_t a = address + i;
        buffer[i] = 0;
        if (a >= RDRAM_END) {
            continue;
        }
        if (!byte_at(native_of(a), &buffer[i])) {
            missed = true;
        }
#ifdef WATCH_TEST
        watch_test_reads++;
        if (watch_test_ram && buffer[i] != watch_test_ram[native_of(a)]) {
            watch_test_wrong++;
        }
#endif
    }
    return num_bytes;
}

static int by_value (const void *a, const void *b) {
    uint32_t x = *(const uint32_t *) a, y = *(const uint32_t *) b;
    return (x > y) - (x < y);
}

/*
 * A pointer chain the hook can follow, from the memory read it ends in back to
 * a fixed address: each step reads a 24-bit pointer (in the swapped view), now
 * or as last frame had it, and adds a constant. Out: the hook's entries, root
 * first, the copy last.
 */
static int chain_entries (const rc_modified_memref_t *node, hook_entry_t *out) {
    if (view != WATCH_VIEW_SWAPPED || node->modifier.type != RC_OPERAND_CONST) {
        return 0;
    }
    uint32_t size = rc_memref_bytes(node->memref.value.size);
    uint32_t c = node->modifier.value.num;
    if (size == 0 || c >= RDRAM_END) {
        return 0;
    }
    hook_entry_t steps[MAX_DEPTH];
    int depth = 0;
    const rc_operand_t *op = &node->parent;
    for (;;) {
        if (depth == MAX_DEPTH - 1 || (op->type != RC_OPERAND_ADDRESS && op->type != RC_OPERAND_DELTA)) {
            return 0;
        }
        const rc_memref_t *m = op->value.memref;
        if (op->size != RC_MEMSIZE_24_BITS) {      /* the pointer as used (rcheevos may read the word whole) */
            return 0;
        }
        uint32_t last = (op->type == RC_OPERAND_DELTA) ? HOOK_LAST : 0;
        if (m->value.memref_type == RC_MEMREF_TYPE_MEMREF) {
            if (m->address >= RDRAM_END) {
                return 0;
            }
            steps[depth].address = m->address | HOOK_POINTER | last;
            steps[depth].length = 0;
            depth++;
            break;
        }
        if (m->value.memref_type != RC_MEMREF_TYPE_MODIFIED_MEMREF) {
            return 0;
        }
        const rc_modified_memref_t *parent = (const rc_modified_memref_t *) m;
        if (parent->modifier_type != RC_OPERATOR_INDIRECT_READ || parent->modifier.type != RC_OPERAND_CONST
                || parent->modifier.value.num >= RDRAM_END) {
            return 0;
        }
        steps[depth].address = parent->modifier.value.num | HOOK_POINTER | last;
        steps[depth].length = 0;
        depth++;
        op = &parent->parent;
    }
    int n = 0;
    for (int i = depth - 1; i >= 0; i--) {
        out[n++] = steps[i];
    }
    /* The copy: the words holding view bytes c .. c + size - 1 past the last pointer. The hook
       takes base + c down to its word, so a value wider than a byte may run into the next one. */
    out[n].address = c | HOOK_TELL;
    out[n].length = (size == 1) ? 4 : 8;
    return n + 1;
}

/* Every fixed address the achievements read, packed into at most MAX_RANGES ranges. */
static void plan_ranges (void) {
    word_count = 0;
    for (const rc_memref_list_t *l = &runtime.memrefs->memrefs; l; l = l->next) {
        for (int i = 0; i < l->count; i++) {
            const rc_memref_t *m = &l->items[i];
            uint32_t size = rc_memref_bytes(m->value.size);
            for (uint32_t b = 0; b < size; b++) {
                if (m->address + b < RDRAM_END) {
                    note_word(native_of(m->address + b) & ~3UL);
                }
            }
        }
    }
    qsort(words, word_count, sizeof(uint32_t), by_value);
    int unique = 0;
    for (int i = 0; i < word_count; i++) {
        if (unique == 0 || words[unique - 1] != words[i]) {
            words[unique++] = words[i];
        }
    }
    word_count = unique;
    /* Merge neighbours, widening the allowed gap until it fits. */
    for (uint32_t gap = 16; ; gap *= 2) {
        int count = 0, bytes = 0;
        for (int i = 0; i < word_count; ) {
            uint32_t start = words[i], end = words[i] + 4;
            while (i + 1 < word_count && words[i + 1] <= end + gap) {
                end = words[++i] + 4;
            }
            i++;
            if (count < MAX_RANGES) {
                list[count].address = start;
                list[count].length = end - start;
                range_offset[count] = bytes;
            }
            bytes += end - start;
            count++;
        }
        if (count <= MAX_RANGES || gap > RDRAM_END) {
            range_count = (count > MAX_RANGES) ? MAX_RANGES : count;
            list_bytes = bytes;
            break;
        }
    }
}

/* The hook's entries for every pointer chain, after the fixed ranges. */
static void plan_chains (void) {
    list_count = range_count;
    chain_count = 0;
    chains_left_out = 0;
    for (const rc_modified_memref_list_t *l = &runtime.memrefs->modified_memrefs; l; l = l->next) {
        for (int i = 0; i < l->count; i++) {
            const rc_modified_memref_t *node = &l->items[i];
            if (node->modifier_type != RC_OPERATOR_INDIRECT_READ) {
                continue;
            }
            hook_entry_t entries[MAX_DEPTH + 1];
            int n = chain_entries(node, entries);
            uint32_t bytes = n ? 4 + entries[n - 1].length : 0;
            if (n == 0 || list_count + n > HOOK_MAX_ENTRIES || list_bytes + bytes > HOOK_MAX_BYTES) {
                chains_left_out++;      /* its reads come back unresolved (counted), never guessed */
                continue;
            }
            memcpy(&list[list_count], entries, n * sizeof(hook_entry_t));
            list_count += n;
            chain_offset[chain_count] = list_bytes;
            chain_length[chain_count] = entries[n - 1].length;
            chain_count++;
            list_bytes += bytes;
        }
    }
}

/* Whether every pointer chain in one achievement can be followed by the hook. */
static bool chains_supported (const char *definition) {
    rc_runtime_t alone;
    rc_runtime_init(&alone);
    bool ok = (rc_runtime_activate_achievement(&alone, 1, definition, NULL, 0) == RC_OK);
    for (const rc_modified_memref_list_t *l = ok ? &alone.memrefs->modified_memrefs : NULL; l && ok; l = l->next) {
        for (int i = 0; i < l->count && ok; i++) {
            const rc_modified_memref_t *node = &l->items[i];
            hook_entry_t entries[MAX_DEPTH + 1];
            if (node->modifier_type == RC_OPERATOR_INDIRECT_READ && chain_entries(node, entries) == 0) {
                ok = false;
            }
        }
    }
    rc_runtime_destroy(&alone);
    return ok;
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
    if (event->type != RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED) {
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
        /* Pointers the hook can't follow would be read wrongly: not checked, rather than checked wrongly. */
        if (!chains_supported(d->definition)) {
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
    plan_ranges();
    plan_chains();
    if (chains_left_out) {
        printf("Note: %d pointer chains didn't fit in the hook's list; reads through them won't be found.\n", chains_left_out);
    }
    if (list_bytes > HOOK_MAX_BYTES) {
        printf("Note: the achievements read %d bytes a frame, more than the hook's %d: some won't be checked.\n",
               list_bytes, HOOK_MAX_BYTES);
    }
    return hook_set_list(list, list_count);
}

const hook_entry_t *watch_list (int *count) {
    *count = list_count;
    return list;
}

void watch_frame (uint32_t frame, const uint8_t *data, uint32_t length, void *context) {
    (void) context;
    frame_data = data;
    frame_length = length;
    frame_now = frame;
    for (int i = 0; i < chain_count; i++) {
        chain_address[i] = (chain_offset[i] + 4 <= length) ? get32(data + chain_offset[i]) : 0xFFFFFFFFUL;
    }
    missed = false;
    rc_runtime_do_frame(&runtime, on_event, read_memory, NULL, NULL);
    if (missed) {
        misses++;
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

void watch_list_size (int *ranges, int *chains, int *bytes) {
    *ranges = range_count;
    *chains = chain_count;
    *bytes = list_bytes;
}
