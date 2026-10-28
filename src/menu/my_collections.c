/**
 * @file my_collections.c
 * @brief Collections the player makes: a name and a list of games.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "my_collections.h"
#include "profiles.h"
#include "safe_file.h"

#define FILE_PATH  profiles_file("collections.txt")   /* the playing profile's */
#define FILE_TMP_PATH  profiles_file("collections.tmp")
#define FILE_HEADER     "collections 1"

typedef struct {
    char name[MY_COLLECTIONS_NAME + 1];
    int count;
    char *games[MY_COLLECTIONS_GAMES];      /* from the top of the card, "N64(US)/Game.z64" */
    uint32_t keys[MY_COLLECTIONS_GAMES];    /* a hash of each, for quick looking up */
} collection_t;

static collection_t *list[MY_COLLECTIONS_MAX];  /* allocated as they are made: none costs nothing */
static int count = 0;
static bool loaded = false;
static int loaded_for = 0;     /* profiles_generation() when loaded */

/* "sd:/N64(US)/Game.z64" -> "N64(US)/Game.z64"; other names as they are. */
static const char *from_top (const char *path) {
    const char *colon = strstr(path, ":/");
    return colon ? colon + 2 : path;
}

static uint32_t key_of (const char *name) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) name; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h;
}

static collection_t *make (const char *name) {
    collection_t *c = calloc(1, sizeof(collection_t));
    if (c) {
        snprintf(c->name, sizeof(c->name), "%s", name);
    }
    return c;
}

static bool add_game (collection_t *c, const char *name) {
    if (c->count >= MY_COLLECTIONS_GAMES) {
        return false;
    }
    char *copy = strdup(name);
    if (!copy) {
        return false;
    }
    c->games[c->count] = copy;
    c->keys[c->count] = key_of(name);
    c->count++;
    return true;
}

static void load (void) {
    if (loaded && loaded_for == profiles_generation()) {
        return;
    }
    loaded = true;
    loaded_for = profiles_generation();   /* another player: forget the last one's */
    for (int i = 0; i < count; i++) {
        for (int g = 0; g < list[i]->count; g++) {
            free(list[i]->games[g]);
        }
        free(list[i]);
        list[i] = NULL;
    }
    count = 0;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(FILE_PATH, "r");
    if (!f) {
        f = fopen(FILE_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }
    char line[300];
    if (fgets(line, sizeof(line), f) && strncmp(line, FILE_HEADER, strlen(FILE_HEADER)) == 0) {
        collection_t *c = NULL;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!line[0] || line[0] == '#') {
                continue;
            }
            if (line[0] == '=' && line[1] == ' ') {
                c = (count < MY_COLLECTIONS_MAX) ? make(line + 2) : NULL;
                if (c) {
                    list[count++] = c;
                }
            } else if (c) {
                add_game(c, line);
            }
        }
    }
    fclose(f);
    debugf("my collections: %d\n", count);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (void) {
    FILE *f = fopen(FILE_TMP_PATH, "w");
    if (!f) {
        debugf("my collections: could not write %s\n", FILE_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", FILE_HEADER);
    for (int k = 0; k < count; k++) {
        fprintf(f, "= %s\n", list[k]->name);
        for (int i = 0; i < list[k]->count; i++) {
            fprintf(f, "%s\n", list[k]->games[i]);
        }
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(FILE_TMP_PATH, FILE_PATH)) {
        debugf("my collections: could not replace %s\n", FILE_PATH);
    }
}

int my_collections_count (void) {
    load();
    return count;
}

const char *my_collections_name (int k) {
    load();
    return (k >= 0 && k < count) ? list[k]->name : "";
}

int my_collections_games (int k) {
    load();
    return (k >= 0 && k < count) ? list[k]->count : 0;
}

int my_collections_create (const char *name) {
    load();
    if (count >= MY_COLLECTIONS_MAX) {
        return -1;
    }
    collection_t *c = make(name);
    if (!c) {
        return -1;
    }
    list[count++] = c;      /* at the end, so the others keep their places */
    debugf("my collections: made \"%s\"\n", name);
    save();
    return count - 1;
}

void my_collections_rename (int k, const char *name) {
    load();
    if (k >= 0 && k < count) {
        snprintf(list[k]->name, sizeof(list[k]->name), "%s", name);
        save();
    }
}

void my_collections_delete (int k) {
    load();
    if (k < 0 || k >= count) {
        return;
    }
    debugf("my collections: deleted \"%s\"\n", list[k]->name);
    for (int i = 0; i < list[k]->count; i++) {
        free(list[k]->games[i]);
    }
    free(list[k]);
    memmove(&list[k], &list[k + 1], (count - k - 1) * sizeof(collection_t *));
    count--;
    save();
}

static int find (const collection_t *c, const char *name) {
    uint32_t key = key_of(name);
    for (int i = 0; i < c->count; i++) {
        if (c->keys[i] == key && strcmp(c->games[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

bool my_collections_has (int k, const char *path) {
    load();
    return path && k >= 0 && k < count && find(list[k], from_top(path)) >= 0;
}

bool my_collections_set (int k, const char *path, bool in) {
    load();
    if (!path || k < 0 || k >= count) {
        return false;
    }
    collection_t *c = list[k];
    const char *name = from_top(path);
    int at = find(c, name);
    if (in && at < 0) {
        if (!add_game(c, name)) {
            return false;
        }
        save();
    } else if (!in && at >= 0) {
        free(c->games[at]);
        memmove(&c->games[at], &c->games[at + 1], (c->count - at - 1) * sizeof(char *));
        memmove(&c->keys[at], &c->keys[at + 1], (c->count - at - 1) * sizeof(uint32_t));
        c->count--;
        save();
    }
    return true;
}

uint32_t my_collections_of (const char *path) {
    load();
    uint32_t in = 0;
    if (!path) {
        return 0;
    }
    const char *name = from_top(path);
    for (int k = 0; k < count; k++) {
        if (find(list[k], name) >= 0) {
            in |= 1u << k;
        }
    }
    return in;
}

void my_collections_rename_game (const char *old_path, const char *new_path) {
    load();
    bool changed = false;
    for (int k = 0; k < count; k++) {
        int i = find(list[k], from_top(old_path));
        char *copy = (i >= 0) ? strdup(from_top(new_path)) : NULL;
        if (copy) {
            free(list[k]->games[i]);
            list[k]->games[i] = copy;
            list[k]->keys[i] = key_of(copy);
            changed = true;
        }
    }
    if (changed) {
        save();
    }
}
