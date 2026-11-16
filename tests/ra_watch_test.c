/*
 * The RetroAchievements bridge's planning and checking (pc/ra_bridge/watch.c)
 * against a stand-in for the game's hook (src/boot/ra_hook.c: the same
 * entries, followed the same way) over a made-up 8 MB of console memory whose
 * pointers move every frame. Every byte rcheevos reads through the bridge must
 * be the byte that memory holds that frame, and none may be missing.
 *
 *   ra_watch_test                         made-up achievements (in tests/run.sh)
 *   ra_watch_test ra_bridge_game_N.txt    also a real set the bridge saved
 *                                         (never commit those: RetroAchievements' data)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook_ring.h"
#include "watch.h"

extern const uint8_t *watch_test_ram;
extern uint32_t watch_test_reads, watch_test_wrong;

#define RAM_SIZE    (8u << 20)

static uint8_t ram[RAM_SIZE];               /* the console's memory, big-endian as it is */
static hook_entry_t hook_list[HOOK_MAX_ENTRIES];
static int hook_count = 0;
static uint32_t copy_bytes = 0;
static int failures = 0;

/* hook_ring.c's part, taken by the test: keep the list as the hook would. */
bool hook_set_list (const hook_entry_t *entries, int count) {
    memcpy(hook_list, entries, count * sizeof(hook_entry_t));
    hook_count = count;
    copy_bytes = 0;
    for (int i = 0; i < count; i++) {
        if (!(entries[i].address & HOOK_POINTER)) {
            copy_bytes += entries[i].length + ((entries[i].address & HOOK_TELL) ? 4 : 0);
        }
    }
    if (copy_bytes > HOOK_MAX_BYTES) {
        copy_bytes = HOOK_MAX_BYTES;
    }
    return true;
}

static void put_word (uint32_t a, uint32_t v) {
    ram[a] = v >> 24; ram[a + 1] = v >> 16; ram[a + 2] = v >> 8; ram[a + 3] = v;
}

/* One frame of the hook, as src/boot/ra_hook.c does it. */
static uint32_t hook_frame (uint8_t *slot) {
    uint32_t room = 4092, at = 0, base = 0;
    for (int i = 0; i < hook_count; i++) {
        hook_entry_t *e = &hook_list[i];
        uint32_t sum = (e->address & 0x007FFFFF) + base, k = sum & 0x007FFFFC;
        if (e->address & HOOK_POINTER) {
            uint32_t v = 0;     /* the 24-bit value in the swapped view; past the end: 0 */
            for (int b = 2; b >= 0 && sum < RAM_SIZE; b--) {
                v = (v << 8) | ram[((sum + b) ^ 3) & (RAM_SIZE - 1)];
            }
            base = v;
            if (e->address & HOOK_LAST) {
                base = e->length;
                e->length = v;
            }
            continue;
        }
        base = 0;
        if (e->address & HOOK_TELL) {
            if ((int32_t) room <= 0) {
                break;
            }
            slot[at] = k >> 24; slot[at + 1] = k >> 16; slot[at + 2] = k >> 8; slot[at + 3] = k;
            at += 4;
            room -= 4;
        }
        for (uint32_t left = e->length; left > 0 && (int32_t) room > 0; left -= 4) {
            memcpy(slot + at, ram + k, 4);
            k += 4;
            at += 4;
            room -= 4;
        }
    }
    return at;
}

/* Words that look like the game's pointers (0x80xxxxxx, word-aligned), with some other values. */
static uint32_t pointer_like (void) {
    if (rand() % 10 < 7) {
        return 0x80000000UL | (((uint32_t) rand() * 4u) & 0x007FFF00UL) | ((rand() % 64) * 4);
    }
    return (uint32_t) rand() * 2654435761u;
}

static void check (int ok, const char *what) {
    if (!ok) {
        printf("  FAIL: %s\n", what);
        failures++;
    }
}

static void run (const char *name, rc_api_achievement_definition_t *defs, int count, int expect_active, int expect_skipped) {
    rc_api_fetch_game_data_response_t game = { 0 };
    game.achievements = defs;
    game.num_achievements = count;
    srand(64);
    for (uint32_t a = 0; a < RAM_SIZE; a += 4) {
        put_word(a, pointer_like());
    }
    int active = watch_load(&game, WATCH_VIEW_SWAPPED);
    watch_plan();
    int ranges, chains, bytes;
    watch_list_size(&ranges, &chains, &bytes);
    printf("%s: %d checked, %d skipped; %d ranges, %d chains, %d bytes a frame, %d list entries\n",
           name, active, watch_skipped(), ranges, chains, bytes, hook_count);
    if (expect_active >= 0) {
        check(active == expect_active, "achievements checked");
        check(watch_skipped() == expect_skipped, "achievements skipped");
    }
    check(bytes <= HOOK_MAX_BYTES && (uint32_t) bytes == copy_bytes, "a frame's copy fits and matches the list");

    /* The pointers the chains start from, so they can be moved every frame. */
    uint32_t roots[HOOK_MAX_ENTRIES];
    int root_count = 0;
    bool chain_start = true;
    for (int i = 0; i < hook_count; i++) {
        if ((hook_list[i].address & HOOK_POINTER) && chain_start) {
            roots[root_count++] = hook_list[i].address & 0x007FFFFC;
        }
        chain_start = !(hook_list[i].address & HOOK_POINTER);
    }

    static uint8_t slot[4096];
    watch_test_ram = ram;
    watch_test_reads = watch_test_wrong = 0;
    uint32_t misses_before = watch_misses();
    for (uint32_t frame = 1; frame <= 300; frame++) {
        for (int i = 0; i < 20000; i++) {
            put_word(((uint32_t) rand() * 4u) % RAM_SIZE, pointer_like());
        }
        for (int i = 0; i < root_count; i++) {
            if (rand() % 3 == 0) {
                put_word(roots[i], pointer_like());
            }
        }
        uint32_t length = hook_frame(slot);
        check(length == copy_bytes, "the hook's copy has the planned size");
        watch_frame(frame, slot, length, NULL);
    }
    printf("  %u bytes read over 300 frames, %u wrong, %u frames with reads not found\n",
           watch_test_reads, watch_test_wrong, watch_misses() - misses_before);
    check(watch_test_reads > 0, "something was read");
    check(watch_test_wrong == 0, "every byte read is the console's");
    check(watch_misses() == misses_before, "no read went unfound");
    watch_test_ram = NULL;
}

static rc_api_achievement_definition_t def (uint32_t id, const char *definition) {
    rc_api_achievement_definition_t d = { 0 };
    d.id = id;
    d.category = RC_ACHIEVEMENT_CATEGORY_PROMOTED;
    d.title = "test";
    d.description = definition;
    d.definition = definition;
    return d;
}

/* A set the bridge saved: "id<TAB>promoted<TAB>...", then two tab-led lines, description and definition. */
static int load_saved (const char *path, rc_api_achievement_definition_t **out) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    static char text[4 << 20];
    size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = 0;
    rc_api_achievement_definition_t *defs = calloc(1024, sizeof(*defs));
    int count = 0, lines_after = -1;
    uint32_t id = 0;
    bool promoted = false;
    for (char *line = strtok(text, "\n"); line && count < 1024; line = strtok(NULL, "\n")) {
        line[strcspn(line, "\r")] = 0;
        unsigned x;
        char cat[16];
        if (line[0] != '\t' && sscanf(line, "%u\t%15s", &x, cat) == 2) {
            id = x;
            promoted = (strcmp(cat, "promoted") == 0);
            lines_after = 0;
        } else if (line[0] == '\t' && lines_after >= 0 && ++lines_after == 2 && promoted) {
            defs[count] = def(id, line + 1);
            count++;
        }
    }
    *out = defs;
    return count;
}

int main (int argc, char **argv) {
    rc_api_achievement_definition_t made_up[] = {
        def(1, "0xH001234=5"),                                  /* a fixed address */
        def(2, "I:0xW100000_0xH000076=3"),                      /* one pointer */
        def(3, "I:0xW100004_I:0xW000084_0x 00020e>100"),        /* two, ending in 16 bits */
        def(4, "I:d0xW100008_0xX000068=1"),                     /* last frame's pointer */
        def(5, "I:0xW10000c_I:d0xW000068_0xH0000f4=2"),         /* last frame's pointer, one step in */
        def(6, "I:0xW100010_0xH00018b=9_0xH002000=1"),          /* an odd offset, and a fixed address too */
        def(7, "I:0xX100014_0xH000010=1"),                      /* 32-bit pointer: not followed */
        def(8, "I:p0xW100018_0xH000010=1"),                     /* prior pointer: not followed */
        def(9, "I:0xW100001_I:0xW000085_0xH000000=1"),          /* pointers at odd places */
        def(101000001, "1=1.300"),                              /* the server's own warning: never checked */
    };
    run("made-up set", made_up, (int) (sizeof(made_up) / sizeof(made_up[0])), 7, 2);

    for (int i = 1; i < argc; i++) {
        rc_api_achievement_definition_t *defs;
        int count = load_saved(argv[i], &defs);
        if (count < 0) {
            printf("%s: can't read it\n", argv[i]);
            failures++;
            continue;
        }
        run(argv[i], defs, count, -1, -1);
        free(defs);
    }

    printf(failures ? "ra_watch: %d FAILED\n" : "ra_watch: all passed\n", failures);
    return failures ? 1 : 0;
}
