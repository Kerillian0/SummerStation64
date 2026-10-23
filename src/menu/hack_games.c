/**
 * @file hack_games.c
 * @brief Hacks that act as games of their own.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "dump_check.h"
#include "hack_games.h"

#define HACKS_PATH      "sd:/menu/hacks.txt"
#define MAX_HACKS       (64)
#define NAME_LENGTH     (96)

static struct {
    uint32_t size;
    uint32_t crc;           /* 0: match by name */
    char code[4];
    char name[NAME_LENGTH]; /* the file's name, without its folder */
} hacks[MAX_HACKS];
static int count = 0;
static bool loaded = false;

static bool code_ok (const char *code) {
    for (int i = 0; i < 4; i++) {
        if (!isalnum((unsigned char) code[i])) {
            return false;
        }
    }
    return true;
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;
    FILE *f = fopen(HACKS_PATH, "r");
    if (!f) {
        debugf("hack games: no %s\n", HACKS_PATH);
        return;
    }
    char line[160];
    bool header = false;
    while (count < MAX_HACKS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!header) {
            header = (strncmp(line, "hacks 1", 7) == 0);
            if (!header) {
                debugf("hack games: %s doesn't start with \"hacks 1\"\n", HACKS_PATH);
                break;      /* not a file this menu knows */
            }
            continue;
        }
        if (line[0] == ';' || line[0] == '\0') {
            continue;
        }
        unsigned long size, crc;
        char code[5];
        int used = 0;
        if (sscanf(line, "%lu %lx %4s %n", &size, &crc, code, &used) == 3 && used > 0 && code_ok(code) && line[used]) {
            hacks[count].size = (uint32_t) size;
            hacks[count].crc = (uint32_t) crc;
            memcpy(hacks[count].code, code, 4);
            snprintf(hacks[count].name, NAME_LENGTH, "%s", &line[used]);
            count++;
        }
    }
    fclose(f);
    debugf("hack games: %d hacks with codes of their own\n", count);
}

bool hack_games_code (const char *path, int64_t size, char *code) {
    load();
    if (count == 0 || !path) {
        return false;
    }
    uint32_t crc = 0;
    bool known_crc = dump_check_crc(path, size, &crc);
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    for (int i = 0; i < count; i++) {
        bool size_ok = (size <= 0) || (hacks[i].size == (uint32_t) size);
        bool match = (known_crc && hacks[i].crc != 0)
            ? (hacks[i].crc == crc && size_ok)
            : (size_ok && strcasecmp(hacks[i].name, name) == 0);
        if (match) {
            memcpy(code, hacks[i].code, 4);
            return true;
        }
    }
    return false;
}
