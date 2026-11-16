/**
 * @file achievements.c
 * @brief A game's achievements from the SD card (see achievements.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdragon.h>

#include "achievements.h"
#include "profiles.h"

#define DIR             "sd:/menu/achievements"
#define UNLOCKED_PATH   profiles_file("achievements.txt")   /* the playing profile's */
#define MAX_SET         (512)
#define MAX_UNLOCKED    (512)

/* The card's index: ROM path from the card's top -> game id. */
typedef struct {
    char *path;
    uint32_t game;
} rom_entry_t;

static rom_entry_t *roms = NULL;
static int rom_count = 0;
static bool index_read = false;

/* The loaded set: the file's text, cut into fields in place. */
static char *set_text = NULL;
static achievement_t *set = NULL;
static int set_count = 0;
static uint32_t set_game = 0;
static const char *set_title = "";

/* What the player has unlocked in the loaded game. */
static struct { uint32_t id; uint16_t day; } unlocked[MAX_UNLOCKED];
static int unlocked_count = 0;

/* A whole file in memory, NUL-terminated. */
static char *read_all (const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (size > 0 && size < 1024 * 1024) ? malloc(size + 1) : NULL;
    if (text) {
        size_t n = fread(text, 1, size, f);
        text[n] = '\0';
    }
    fclose(f);
    return text;
}

/* The next line, cut off in place; NULL at the end. */
static char *next_line (char **at) {
    if (!*at || !**at) {
        return NULL;
    }
    char *line = *at;
    char *end = strchr(line, '\n');
    if (end) {
        *end = '\0';
        *at = end + 1;
    } else {
        *at = line + strlen(line);
    }
    size_t n = strlen(line);
    if (n && line[n - 1] == '\r') {
        line[n - 1] = '\0';
    }
    return line;
}

/* The next tab-separated field, cut off in place. */
static char *next_field (char **at) {
    char *field = *at;
    char *tab = strchr(field, '\t');
    if (tab) {
        *tab = '\0';
        *at = tab + 1;
    } else {
        *at = field + strlen(field);
    }
    return field;
}

static void read_index (void) {
    index_read = true;
    char *text = read_all(DIR "/index.txt");
    if (!text) {
        return;
    }
    char *at = text, *line = next_line(&at);
    if (!line || strcmp(line, "achindex 1") != 0) {
        free(text);
        return;
    }
    int lines = 0;
    for (char *c = at; *c; c++) {
        lines += (*c == '\n');
    }
    roms = calloc(lines + 1, sizeof(rom_entry_t));
    while (roms && (line = next_line(&at)) != NULL) {
        next_field(&line);                          /* size */
        next_field(&line);                          /* md5 */
        uint32_t game = strtoul(next_field(&line), NULL, 10);
        if (*line && game != 0) {                   /* the rest is the path */
            roms[rom_count].path = strdup(line);
            roms[rom_count].game = game;
            if (roms[rom_count].path) {
                rom_count++;
            }
        }
    }
    free(text);
    debugf("achievements: %d games on the card's list\n", rom_count);
}

ach_kind_t achievements_for_rom (const char *path, uint32_t *game_id) {
    *game_id = 0;
    if (!index_read) {
        read_index();
    }
    const char *colon = strchr(path, ':');          /* "sd:/N64(US)/Game.z64" -> "N64(US)/Game.z64" */
    const char *name = colon ? colon + 1 : path;
    while (*name == '/') {
        name++;
    }
    for (int i = 0; i < rom_count; i++) {
        if (strcmp(roms[i].path, name) == 0) {
            *game_id = roms[i].game;
            return (roms[i].game >= 1000000000UL) ? ACH_OTHER_DUMP : ACH_SET;
        }
    }
    return ACH_NONE;
}

static void read_unlocked (void) {
    unlocked_count = 0;
    char *text = read_all(UNLOCKED_PATH);
    if (!text) {
        return;
    }
    char *at = text, *line = next_line(&at);
    if (line && strcmp(line, "achunlocked 1") == 0) {
        while ((line = next_line(&at)) != NULL && unlocked_count < MAX_UNLOCKED) {
            uint32_t game = strtoul(next_field(&line), NULL, 10);
            uint32_t id = strtoul(next_field(&line), NULL, 10);
            if (game == set_game && id != 0) {
                unlocked[unlocked_count].id = id;
                unlocked[unlocked_count].day = (uint16_t) strtoul(next_field(&line), NULL, 10);
                unlocked_count++;
            }
        }
    }
    free(text);
}

void achievements_set_free (void) {
    free(set_text);
    free(set);
    set_text = NULL;
    set = NULL;
    set_count = 0;
    set_game = 0;
    set_title = "";
    unlocked_count = 0;
}

bool achievements_set_load (uint32_t game_id) {
    achievements_set_free();
    char path[64];
    snprintf(path, sizeof(path), DIR "/%lu/set.txt", (unsigned long) game_id);
    set_text = read_all(path);
    if (!set_text) {
        debugf("achievements: no %s\n", path);
        return false;
    }
    char *at = set_text, *line = next_line(&at);
    if (!line || strcmp(line, "achset 1") != 0) {
        achievements_set_free();
        return false;
    }
    set = calloc(MAX_SET, sizeof(achievement_t));
    if (!set) {
        achievements_set_free();
        return false;
    }
    set_game = game_id;
    while ((line = next_line(&at)) != NULL) {
        char *kind = next_field(&line);
        if (strcmp(kind, "game") == 0) {
            next_field(&line);
            set_title = line;
        } else if (strcmp(kind, "a") == 0 && set_count < MAX_SET) {
            achievement_t *a = &set[set_count];
            a->id = strtoul(next_field(&line), NULL, 10);
            a->points = (uint16_t) strtoul(next_field(&line), NULL, 10);
            a->type = (uint8_t) strtoul(next_field(&line), NULL, 10);
            a->badge = next_field(&line);
            a->title = next_field(&line);
            a->description = line;
            if (a->id) {
                set_count++;
            }
        }
    }
    read_unlocked();
    debugf("achievements: %s, %d achievements, %d unlocked\n", set_title, set_count, unlocked_count);
    return true;
}

const char *achievements_set_title (void) {
    return set_title;
}

int achievements_count (void) {
    return set_count;
}

const achievement_t *achievements_get (int i) {
    return (i >= 0 && i < set_count) ? &set[i] : NULL;
}

bool achievements_unlocked (int i, uint16_t *day) {
    if (i < 0 || i >= set_count) {
        return false;
    }
    for (int u = 0; u < unlocked_count; u++) {
        if (unlocked[u].id == set[i].id) {
            if (day) {
                *day = unlocked[u].day;
            }
            return true;
        }
    }
    return false;
}

void achievements_progress (int *count, int *points, int *total_points) {
    *count = *points = *total_points = 0;
    for (int i = 0; i < set_count; i++) {
        *total_points += set[i].points;
        if (achievements_unlocked(i, NULL)) {
            (*count)++;
            *points += set[i].points;
        }
    }
}
