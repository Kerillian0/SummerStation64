/**
 * @file game_index.c
 * @brief A remembered list of which file is which game.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "game_index.h"
#include "safe_file.h"

#define INDEX_PATH      "sd:/menu/gameindex.txt"
#define INDEX_TMP_PATH  "sd:/menu/gameindex.tmp"
#define INDEX_HEADER    "gameindex 1"
#define MAX_GAMES       (768)       /* 12 KB of memory; the oldest notes are written over after that */

#define FLAG_FACTS      (1 << 0)
#define FLAG_NEEDS      (1 << 1)
#define FLAG_LIKES      (1 << 2)
#define FLAG_SAVES      (1 << 3)

typedef struct {
    uint32_t key;       /* made from the file's full path; 0 = empty */
    uint32_t size;      /* the file's size, 0 if it was not known */
    char code[4];
    int8_t players;
    uint8_t flags;
} note_t;

static note_t notes[MAX_GAMES];
static int count = 0;
static int next = 0;        /* where the next new note goes once the list is full */
static bool loaded = false;
static bool dirty = false;
static game_index_entry_t answer;

static uint32_t path_key (const char *path) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) path; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h ? h : 1;
}

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

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(INDEX_PATH, "r");
    if (!f) {
        f = fopen(INDEX_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }

    char line[64];
    if (fgets(line, sizeof(line), f) && strncmp(line, INDEX_HEADER, strlen(INDEX_HEADER)) == 0) {
        while (count < MAX_GAMES && fgets(line, sizeof(line), f)) {
            unsigned long key, size;
            char code[8];
            int players, flags;
            if (sscanf(line, "%lx %lu %4s %d %d", &key, &size, code, &players, &flags) == 5 && key != 0 && strlen(code) == 4 && code_ok(code)) {
                note_t *n = &notes[count++];
                n->key = (uint32_t) key;
                n->size = (uint32_t) size;
                memcpy(n->code, code, 4);
                n->players = (int8_t) players;
                n->flags = (uint8_t) flags;
            }
        }
    }
    fclose(f);
    next = count % MAX_GAMES;
    debugf("game index: %d games remembered\n", count);
}

static note_t *find (uint32_t key, int64_t size) {
    for (int i = 0; i < count; i++) {
        if (notes[i].key == key) {
            /* Same name but another size: the file was replaced. */
            if (size > 0 && notes[i].size != 0 && notes[i].size != (uint32_t) size) {
                return NULL;
            }
            return &notes[i];
        }
    }
    return NULL;
}

/* The note for this file: the one it has, or a fresh one. */
static note_t *note_for (const char *path, int64_t size) {
    load();
    uint32_t key = path_key(path);

    for (int i = 0; i < count; i++) {
        if (notes[i].key == key) {
            if (size > 0 && notes[i].size != 0 && notes[i].size != (uint32_t) size) {
                memset(&notes[i], 0, sizeof(note_t));   /* replaced file: start again */
                notes[i].key = key;
            }
            return &notes[i];
        }
    }

    note_t *n;
    if (count < MAX_GAMES) {
        n = &notes[count++];
    } else {
        n = &notes[next];
        next = (next + 1) % MAX_GAMES;
    }
    memset(n, 0, sizeof(note_t));
    n->key = key;
    return n;
}

const game_index_entry_t *game_index_find (const char *path, int64_t size) {
    load();
    note_t *n = find(path_key(path), size);
    if (!n || !code_ok(n->code)) {
        return NULL;
    }
    memcpy(answer.code, n->code, 4);
    answer.has_facts = (n->flags & FLAG_FACTS) != 0;
    answer.players = n->players;
    answer.needs_expansion = (n->flags & FLAG_NEEDS) != 0;
    answer.likes_expansion = (n->flags & FLAG_LIKES) != 0;
    answer.saves = (n->flags & FLAG_SAVES) != 0;
    return &answer;
}

void game_index_set_code (const char *path, int64_t size, const char *code) {
    if (!code_ok(code)) {
        return;
    }
    note_t *n = note_for(path, size);
    if (memcmp(n->code, code, 4) == 0 && (n->size != 0 || size <= 0)) {
        return;     /* nothing new */
    }
    memcpy(n->code, code, 4);
    if (size > 0) {
        n->size = (uint32_t) size;
    }
    dirty = true;
}

void game_index_set_facts (const char *path, int64_t size, const char *code, int players, bool needs_expansion, bool likes_expansion, bool saves) {
    if (!code_ok(code)) {
        return;
    }
    note_t *n = note_for(path, size);
    memcpy(n->code, code, 4);
    if (size > 0) {
        n->size = (uint32_t) size;
    }
    n->players = (int8_t) players;
    n->flags = FLAG_FACTS | (needs_expansion ? FLAG_NEEDS : 0) | (likes_expansion ? FLAG_LIKES : 0) | (saves ? FLAG_SAVES : 0);
    dirty = true;
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
void game_index_flush (void) {
    if (!dirty) {
        return;
    }
    dirty = false;

    FILE *f = fopen(INDEX_TMP_PATH, "w");
    if (!f) {
        debugf("game index: could not write %s\n", INDEX_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", INDEX_HEADER);
    for (int i = 0; i < count; i++) {
        note_t *n = &notes[i];
        if (n->key == 0 || !code_ok(n->code)) {
            continue;
        }
        fprintf(f, "%08lx %lu %c%c%c%c %d %d\n", (unsigned long) n->key, (unsigned long) n->size,
            n->code[0], n->code[1], n->code[2], n->code[3], (int) n->players, (int) n->flags);
    }
    fclose(f);

    if (!safe_file_replace(INDEX_TMP_PATH, INDEX_PATH)) {
        debugf("game index: could not replace %s\n", INDEX_PATH);
    } else {
        debugf("game index: saved %d games\n", count);
    }
}
