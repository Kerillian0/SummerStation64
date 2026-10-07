#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>

#include "menu_features.h"
#include "play_stats.h"
#include "safe_file.h"

/* Not profile-aware yet: see "Decide early" in CLAUDE.md. */
#define STATS_PATH      "sd:/menu/playstats.txt"
#define STATS_TMP_PATH  "sd:/menu/playstats.tmp"

/* 12 bytes a game in memory. When full, the game played longest ago makes way. */
#define MAX_GAMES       (512)

typedef struct {
    uint32_t key;       /* hash of the full path */
    uint32_t count;
    uint32_t last;      /* seconds since 1970, 0 = not known */
} stat_t;

static stat_t stats[MAX_GAMES];
static int stat_count = 0;
static bool loaded = false;

static uint32_t path_hash (const char *path) {
    uint32_t hash = 2166136261u;
    while (*path) {
        hash = (hash ^ (uint8_t) *path++) * 16777619u;
    }
    return hash;
}

/* One game per line: hash in hex, times started, time of the last start. */
static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(STATS_PATH, "r");
    if (!f) {
        f = fopen(STATS_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }

    char line[64];
    while (stat_count < MAX_GAMES && fgets(line, sizeof(line), f)) {
        unsigned long key, count, last;
        if (sscanf(line, "%lx %lu %lu", &key, &count, &last) == 3) {
            stats[stat_count].key = (uint32_t) key;
            stats[stat_count].count = (uint32_t) count;
            stats[stat_count].last = (uint32_t) last;
            stat_count++;
        }
    }
    fclose(f);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (void) {
    FILE *f = fopen(STATS_TMP_PATH, "w");
    if (!f) {
        debugf("play stats: could not write %s\n", STATS_TMP_PATH);
        return;
    }
    for (int i = 0; i < stat_count; i++) {
        fprintf(f, "%08lx %lu %lu\n", (unsigned long) stats[i].key, (unsigned long) stats[i].count, (unsigned long) stats[i].last);
    }
    bool ok = (fclose(f) == 0);

    if (!ok || !safe_file_replace(STATS_TMP_PATH, STATS_PATH)) {
        debugf("play stats: could not replace %s\n", STATS_PATH);
    }
}

static stat_t *find (uint32_t key) {
    for (int i = 0; i < stat_count; i++) {
        if (stats[i].key == key) {
            return &stats[i];
        }
    }
    return NULL;
}

void play_stats_record (const char *rom_path, time_t now) {
    if (!rom_path || !features_enabled(FEATURE_PLAY_STATS)) {
        return;
    }
    load();

    uint32_t key = path_hash(rom_path);
    stat_t *s = find(key);

    if (!s) {
        if (stat_count < MAX_GAMES) {
            s = &stats[stat_count++];
        } else {
            /* Full: reuse the entry that was played longest ago. */
            s = &stats[0];
            for (int i = 1; i < stat_count; i++) {
                if (stats[i].last < s->last) {
                    s = &stats[i];
                }
            }
        }
        s->key = key;
        s->count = 0;
        s->last = 0;
    }

    s->count++;
    if (now > 0) {
        s->last = (uint32_t) now;
    }

    save();
}

int play_stats_get (const char *rom_path, time_t *last) {
    if (last) {
        *last = 0;
    }
    if (!rom_path) {
        return 0;
    }
    load();

    stat_t *s = find(path_hash(rom_path));
    if (!s) {
        return 0;
    }
    if (last) {
        *last = (time_t) s->last;
    }
    return (int) s->count;
}
