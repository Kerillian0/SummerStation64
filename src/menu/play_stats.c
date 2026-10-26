#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "menu_features.h"
#include "play_stats.h"
#include "safe_file.h"

/* Not profile-aware yet. */
#define STATS_PATH      "sd:/menu/playstats.txt"
#define STATS_TMP_PATH  "sd:/menu/playstats.tmp"

/* 16 bytes a game in memory. When full, the game played longest ago makes way. */
#define MAX_GAMES       (512)

/* Longer than this, a session ended by RESET is taken for a power-off (or a clock that jumped). */
#define LONGEST_SESSION (12 * 60 * 60)

/* A session ended by the power switch counts only if the console was back on within this. */
#define LONGEST_AFTER_POWER_OFF (4 * 60 * 60)

#define PATH_LENGTH     (256)

typedef struct {
    uint32_t key;       /* hash of the full path */
    uint32_t count;
    uint32_t last;      /* seconds since 1970, 0 = not known */
    uint32_t seconds;   /* time played */
} stat_t;

static stat_t stats[MAX_GAMES];
static int stat_count = 0;
static bool loaded = false;

/* The session begun at the last game start, until the menu settles it. */
static struct {
    bool open;
    uint32_t start;
    char rom[PATH_LENGTH];
} session;

static uint32_t path_hash (const char *path) {
    uint32_t hash = 2166136261u;
    while (*path) {
        hash = (hash ^ (uint8_t) *path++) * 16777619u;
    }
    return hash;
}

/* One game per line: hash in hex, times started, time of the last start,
   seconds played (missing in files from before). The open session, if any,
   on a line of its own: "session <start> <game path>" (a first build also
   wrote the save's path and a tab before the game's). */
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

    char line[(PATH_LENGTH * 2) + 32];
    while (fgets(line, sizeof(line), f)) {
        unsigned long key, count, last, seconds = 0, start;
        int used = 0;
        if (sscanf(line, "session %lu %n", &start, &used) == 1 && used > 0) {
            line[strcspn(line, "\r\n")] = '\0';
            char *tab = strchr(&line[used], '\t');
            session.open = true;
            session.start = (uint32_t) start;
            snprintf(session.rom, sizeof(session.rom), "%s", tab ? tab + 1 : &line[used]);
        } else if (stat_count < MAX_GAMES && sscanf(line, "%lx %lu %lu %lu", &key, &count, &last, &seconds) >= 3) {
            stats[stat_count].key = (uint32_t) key;
            stats[stat_count].count = (uint32_t) count;
            stats[stat_count].last = (uint32_t) last;
            stats[stat_count].seconds = (uint32_t) seconds;
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
        fprintf(f, "%08lx %lu %lu %lu\n", (unsigned long) stats[i].key, (unsigned long) stats[i].count,
            (unsigned long) stats[i].last, (unsigned long) stats[i].seconds);
    }
    if (session.open) {
        fprintf(f, "session %lu %s\n", (unsigned long) session.start, session.rom);
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
        s->seconds = 0;
    }

    s->count++;
    if (now > 0) {
        s->last = (uint32_t) now;
    }

    /* A session left open (the menu never got to settle it) is dropped. */
    session.open = (now > 0 && strlen(rom_path) < PATH_LENGTH);
    if (session.open) {
        session.start = (uint32_t) now;
        snprintf(session.rom, sizeof(session.rom), "%s", rom_path);
    }

    save();
}

void play_stats_settle (bool power_on) {
    load();
    if (!session.open) {
        return;
    }
    session.open = false;
    time_t now = time(NULL);
    long length = (now > 0) ? (long) (now - (time_t) session.start) : -1;
    long most = power_on ? LONGEST_AFTER_POWER_OFF : LONGEST_SESSION;

    stat_t *s = find(path_hash(session.rom));
    if (s && length > 0 && length <= most && features_enabled(FEATURE_PLAY_STATS)) {
        s->seconds += (uint32_t) length;
        debugf("time played: %s, %ld min (%s), %lu min in all\n", session.rom, length / 60,
            power_on ? "power switch, back on soon" : "reset", (unsigned long) (s->seconds / 60));
    } else {
        debugf("time played: %s, session of %ld min not counted (%s)\n", session.rom, length / 60,
            power_on ? "power switch, off over 4 hours" : "no clock, or over 12 hours");
    }
    save();
}

uint32_t play_stats_seconds (const char *rom_path) {
    if (!rom_path) {
        return 0;
    }
    load();
    stat_t *s = find(path_hash(rom_path));
    return s ? s->seconds : 0;
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

void play_stats_rename (const char *old_path, const char *new_path) {
    load();
    stat_t *s = find(path_hash(old_path));
    if (s && !find(path_hash(new_path))) {
        s->key = path_hash(new_path);
        save();
    }
}
