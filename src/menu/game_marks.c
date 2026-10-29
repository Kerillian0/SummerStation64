#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_marks.h"
#include "safe_file.h"

/* One kind of mark, kept in a file of its own. Every profile's: what a game
   is doesn't change with the player. */
typedef struct {
    const char *path, *tmp_path, *header, *what;
    char *names[GAME_MARKS_MAX];
    int8_t marks[GAME_MARKS_MAX];
    int count;
    bool loaded;
} mark_list_t;

static mark_list_t lists[GAME_MARK_KINDS] = {
    [GAME_MARK_HOMEBREW] = { .path = "sd:/menu/homebrew.txt", .tmp_path = "sd:/menu/homebrew.tmp", .header = "homebrew 1", .what = "homebrew" },
    [GAME_MARK_HACK] = { .path = "sd:/menu/hackmarks.txt", .tmp_path = "sd:/menu/hackmarks.tmp", .header = "hackmarks 1", .what = "a hack" },
};

/* "sd:/N64(US)/Game.z64" -> "N64(US)/Game.z64"; other names as they are. */
static const char *from_top (const char *path) {
    const char *colon = strstr(path, ":/");
    return colon ? colon + 2 : path;
}

static mark_list_t *list_of (game_mark_kind_t kind) {
    mark_list_t *l = &lists[(kind == GAME_MARK_HACK) ? GAME_MARK_HACK : GAME_MARK_HOMEBREW];
    if (l->loaded) {
        return l;
    }
    l->loaded = true;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(l->path, "r");
    if (!f) {
        f = fopen(l->tmp_path, "r");
    }
    if (!f) {
        return l;
    }
    char line[300];
    bool header = false;
    while (fgets(line, sizeof(line), f) && l->count < GAME_MARKS_MAX) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!header) {
            header = (strcmp(line, l->header) == 0);
            if (!header) {
                break;      /* not a file the menu wrote */
            }
            continue;
        }
        if ((line[0] == '+' || line[0] == '-') && line[1] == ' ' && line[2]) {
            l->names[l->count] = strdup(line + 2);
            if (l->names[l->count]) {
                l->marks[l->count++] = (line[0] == '+') ? 1 : -1;
            }
        }
    }
    fclose(f);
    debugf("game marks: %d for %s\n", l->count, l->what);
    return l;
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (mark_list_t *l) {
    FILE *f = fopen(l->tmp_path, "w");
    if (!f) {
        debugf("game marks: could not write %s\n", l->tmp_path);
        return;
    }
    fprintf(f, "%s\n", l->header);
    for (int i = 0; i < l->count; i++) {
        fprintf(f, "%c %s\n", (l->marks[i] > 0) ? '+' : '-', l->names[i]);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(l->tmp_path, l->path)) {
        debugf("game marks: could not replace %s\n", l->path);
    }
}

static int find (mark_list_t *l, const char *name) {
    for (int i = 0; i < l->count; i++) {
        if (strcmp(l->names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int game_marks_get (game_mark_kind_t kind, const char *name) {
    mark_list_t *l = list_of(kind);
    int at = (l->count > 0 && name) ? find(l, name) : -1;
    return (at >= 0) ? l->marks[at] : 0;
}

int game_marks_get_path (game_mark_kind_t kind, const char *path) {
    return path ? game_marks_get(kind, from_top(path)) : 0;
}

static bool set_one (mark_list_t *l, const char *name, int mark) {
    int at = find(l, name);
    if (mark == 0) {
        if (at >= 0) {
            free(l->names[at]);
            memmove(&l->names[at], &l->names[at + 1], (l->count - at - 1) * sizeof(char *));
            memmove(&l->marks[at], &l->marks[at + 1], (l->count - at - 1) * sizeof(int8_t));
            l->count--;
            save(l);
        }
        return true;
    }
    if (at < 0) {
        if (l->count >= GAME_MARKS_MAX || !(l->names[l->count] = strdup(name))) {
            return false;
        }
        at = l->count++;
    } else if (l->marks[at] == mark) {
        return true;
    }
    l->marks[at] = (mark > 0) ? 1 : -1;
    debugf("game marks: %s %s %s\n", name, (mark > 0) ? "is" : "is not", l->what);
    save(l);
    return true;
}

bool game_marks_set (game_mark_kind_t kind, const char *path, int mark) {
    if (!path) {
        return false;
    }
    const char *name = from_top(path);
    game_mark_kind_t other = (kind == GAME_MARK_HACK) ? GAME_MARK_HOMEBREW : GAME_MARK_HACK;
    if (mark > 0 && game_marks_get(other, name) > 0) {
        set_one(list_of(other), name, 0);   /* never both */
    }
    return set_one(list_of(kind), name, mark);
}

void game_marks_rename (const char *old_path, const char *new_path) {
    for (int k = 0; k < GAME_MARK_KINDS; k++) {
        mark_list_t *l = list_of((game_mark_kind_t) k);
        int at = find(l, from_top(old_path));
        if (at >= 0) {
            char *copy = strdup(from_top(new_path));
            if (copy) {
                free(l->names[at]);
                l->names[at] = copy;
                save(l);
            }
        }
    }
}
