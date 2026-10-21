/**
 * @file dump_check.c
 * @brief Whether a game file is a known good dump.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "dump_check.h"
#include "menu_features.h"
#include "rom_hacks.h"

#define VERIFIED_PATH   "sd:/menu/verified.txt"
#define VERIFIED_HEADER "verified 1"
#define MAX_FILES       (768)       /* 9 bytes each: about 7 KB */

static uint32_t keys[MAX_FILES];
static uint32_t sizes[MAX_FILES];
static char results[MAX_FILES];     /* 'V', 'M' or 'U', as Card Prep wrote them */
static int count = 0;
static bool loaded = false;

/* The same sum as the menu's game list and Card Prep use for a path. */
static uint32_t path_key (const char *path) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) path; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h ? h : 1;
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;
    FILE *f = fopen(VERIFIED_PATH, "r");
    if (!f) {
        debugf("dump check: no verified.txt, nothing marked\n");
        return;
    }
    char line[256];
    if (fgets(line, sizeof(line), f) && strncmp(line, VERIFIED_HEADER, strlen(VERIFIED_HEADER)) == 0) {
        while (count < MAX_FILES && fgets(line, sizeof(line), f)) {
            unsigned long key, size, crc;
            char result;
            if (sscanf(line, "%lx %lu %lx %c", &key, &size, &crc, &result) == 4 && (result == 'V' || result == 'M' || result == 'U')) {
                keys[count] = (uint32_t) key;
                sizes[count] = (uint32_t) size;
                results[count] = result;
                count++;
            }
        }
    }
    fclose(f);
    debugf("dump check: %d files from verified.txt\n", count);
}

/* Does the file name (any case) hold this? */
static bool name_has (const char *name, const char *part) {
    size_t n = strlen(part);
    for (const char *c = name; *c; c++) {
        if (strncasecmp(c, part, n) == 0) {
            return true;
        }
    }
    return false;
}

dump_state_t dump_check (const char *path, int64_t size, bool hack) {
    if (!path || !features_enabled(FEATURE_DUMP_MARKS)) {
        return DUMP_NONE;
    }
    load();
    uint32_t key = path_key(path);
    char result = 0;
    for (int i = 0; i < count; i++) {
        if (keys[i] == key && (size < 0 || sizes[i] == (uint32_t) size)) {
            result = results[i];
            break;
        }
    }
    if (result == 'V') {
        return DUMP_VERIFIED;
    }
    if (result != 'M' && result != 'U') {
        return DUMP_NONE;           /* not checked by Card Prep (or replaced since) */
    }

    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    if (name_has(name, "[b")) {
        return DUMP_BAD;            /* the dumper's own "bad dump" tag */
    }
    if (result == 'U') {
        return DUMP_NONE;           /* homebrew, prototypes: No-Intro can't say */
    }
    /* Changed on purpose, so a difference is expected: fixed [f], trainer
       or translation [t] / [T+Eng], hack [h], pirate [p], or said in words. */
    if (hack || rom_hacks_name_says(name) || name_has(name, "[f") || name_has(name, "[t") ||
        name_has(name, "[h") || name_has(name, "[p") || name_has(name, "patched") || name_has(name, "translat")) {
        return DUMP_NONE;
    }
    return DUMP_UNVERIFIED;
}

const char *dump_check_label (dump_state_t state) {
    switch (state) {
        case DUMP_VERIFIED: return "Verified";
        case DUMP_UNVERIFIED: return "Not verified";
        case DUMP_BAD: return "Bad dump";
        default: return NULL;
    }
}
