#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "profiles.h"
#include "safe_file.h"
#include "utils/fs.h"

#define PROFILES_PATH       "sd:/menu/profiles.txt"
#define PROFILES_TMP_PATH   "sd:/menu/profiles.tmp"
#define PROFILES_HEADER     "profiles 1"
#define NAME_LENGTH         (28)    /* the naming dial spells up to 27 characters */
#define RETIRED_MAX         (16)    /* folders of deleted profiles, never handed out again */
#define FIRST_FOLDER        "-"     /* the first profile: saves where they always were */
#define LISTS_DIR           "sd:/menu/profiles"     /* players 2-4: Recent, Favorites, play counts, ... in <folder>/ here */

typedef struct {
    char name[NAME_LENGTH];
    char folder[NAME_LENGTH];
    int color;
} profile_t;

static profile_t list[PROFILES_MAX];
static int count = 0;
static int current = 0;
static char retired[RETIRED_MAX][NAME_LENGTH];
static int retired_count = 0;
static bool loaded = false;
static int generation = 0;     /* goes up whenever the player changes, so lists read the new player's files */

/* Blue, red, green, yellow: the colors of the four controller ports on many pads. */
static const color_t colors[PROFILE_COLORS] = {
    { 0x3C, 0x78, 0xF0, 0xFF },
    { 0xE8, 0x46, 0x3C, 0xFF },
    { 0x3C, 0xB4, 0x5A, 0xFF },
    { 0xF0, 0xC8, 0x3C, 0xFF },
};

static void save (void) {
    FILE *f = fopen(PROFILES_TMP_PATH, "w");
    if (!f) {
        debugf("profiles: could not write %s\n", PROFILES_TMP_PATH);
        return;
    }
    fprintf(f, "%s\ncurrent %d\n", PROFILES_HEADER, current);
    for (int i = 0; i < count; i++) {
        fprintf(f, "profile %d\t%s\t%s\n", list[i].color, list[i].folder, list[i].name);
    }
    for (int i = 0; i < retired_count; i++) {
        fprintf(f, "retired\t%s\n", retired[i]);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(PROFILES_TMP_PATH, PROFILES_PATH)) {
        debugf("profiles: could not replace %s\n", PROFILES_PATH);
    }
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;
    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(PROFILES_PATH, "r");
    if (!f) {
        f = fopen(PROFILES_TMP_PATH, "r");
    }
    if (f) {
        char line[128];
        bool header = false;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!header) {
                header = (strcmp(line, PROFILES_HEADER) == 0);
                if (!header) {
                    break;  /* not a file the menu wrote */
                }
                continue;
            }
            char *tab = strchr(line, '\t');
            if (strncmp(line, "current ", 8) == 0) {
                current = atoi(line + 8);
            } else if (strncmp(line, "profile ", 8) == 0 && tab && count < PROFILES_MAX) {
                char *tab2 = strchr(tab + 1, '\t');
                if (!tab2) {
                    continue;
                }
                *tab2 = '\0';
                profile_t *p = &list[count++];
                p->color = atoi(line + 8) % PROFILE_COLORS;
                snprintf(p->folder, sizeof(p->folder), "%s", tab + 1);
                snprintf(p->name, sizeof(p->name), "%s", tab2 + 1);
            } else if (strncmp(line, "retired\t", 8) == 0 && retired_count < RETIRED_MAX) {
                snprintf(retired[retired_count++], NAME_LENGTH, "%s", line + 8);
            }
        }
        fclose(f);
    }
    if (count == 0 || strcmp(list[0].folder, FIRST_FOLDER) != 0) {
        /* No list yet (or a damaged one): just the first profile. */
        count = 1;
        snprintf(list[0].name, NAME_LENGTH, "Player 1");
        snprintf(list[0].folder, NAME_LENGTH, FIRST_FOLDER);
        list[0].color = 0;
    }
    if (current < 0 || current >= count) {
        current = 0;
    }
    debugf("profiles: %d, playing as %s\n", count, list[current].name);
}

/* Players other than the first keep their lists in a folder of their own. */
static void lists_folder_make (void) {
    if (current > 0) {
        char folder[96];
        snprintf(folder, sizeof(folder), LISTS_DIR "/%s", list[current].folder);
        if (!directory_exists(folder) && directory_create(folder)) {
            debugf("profiles: could not make %s\n", folder);
        }
    }
}

int profiles_generation (void) {
    load();
    return generation;
}

const char *profiles_file (const char *name) {
    static char paths[4][128];
    static int turn = 0;
    load();
    char *out = paths[turn];
    turn = (turn + 1) % 4;
    if (current == 0) {
        snprintf(out, sizeof(paths[0]), "sd:/menu/%s", name);
    } else {
        snprintf(out, sizeof(paths[0]), LISTS_DIR "/%s/%s", list[current].folder, name);
    }
    return out;
}

int profiles_count (void) {
    load();
    return count;
}

int profiles_current (void) {
    load();
    return current;
}

const char *profiles_name (int profile) {
    load();
    return (profile >= 0 && profile < count) ? list[profile].name : "";
}

int profiles_color_index (int profile) {
    load();
    return (profile >= 0 && profile < count) ? list[profile].color : 0;
}

color_t profiles_color (int profile) {
    return colors[profiles_color_index(profile)];
}

void profiles_next_color (int profile) {
    load();
    if (profile >= 0 && profile < count) {
        list[profile].color = (list[profile].color + 1) % PROFILE_COLORS;
        save();
    }
}

void profiles_switch (int profile) {
    load();
    if (profile >= 0 && profile < count && profile != current) {
        current = profile;
        generation++;
        debugf("profiles: playing as %s\n", list[current].name);
        lists_folder_make();
        save();
    }
}

static bool folder_taken (const char *folder) {
    if (strcasecmp(folder, "backups") == 0 || strcmp(folder, FIRST_FOLDER) == 0) {
        return true;    /* the save backups' folder sits beside the profiles' */
    }
    for (int i = 0; i < count; i++) {
        if (strcasecmp(list[i].folder, folder) == 0) {
            return true;
        }
    }
    for (int i = 0; i < retired_count; i++) {
        if (strcasecmp(retired[i], folder) == 0) {
            return true;
        }
    }
    return false;
}

/* A folder name from the player's name: only what every card takes, and one
   no profile has had before ("Alex", "Alex 2", ...). */
static void make_folder (const char *name, char *out) {
    char base[NAME_LENGTH];
    int n = 0;
    for (const char *c = name; *c && n < 20; c++) {
        if (strchr("\\/:*?\"<>|", *c) || (unsigned char) *c < 32) {
            continue;
        }
        base[n++] = *c;
    }
    while (n > 0 && (base[n - 1] == ' ' || base[n - 1] == '.')) {
        n--;    /* a card won't take a name ending in a space or a dot */
    }
    base[n] = '\0';
    if (n == 0 || base[0] == '.') {
        snprintf(base, sizeof(base), "Player");
    }
    snprintf(out, NAME_LENGTH, "%s", base);
    for (int k = 2; folder_taken(out) && k < 100; k++) {
        snprintf(out, NAME_LENGTH, "%s %d", base, k);
    }
}

bool profiles_add (const char *name) {
    load();
    if (count >= PROFILES_MAX || !name || !name[0]) {
        return false;
    }
    profile_t *p = &list[count];
    snprintf(p->name, NAME_LENGTH, "%s", name);
    make_folder(name, p->folder);
    /* The first color nobody has yet. */
    p->color = 0;
    for (int c = 0; c < PROFILE_COLORS; c++) {
        bool used = false;
        for (int i = 0; i < count; i++) {
            used |= (list[i].color == c);
        }
        if (!used) {
            p->color = c;
            break;
        }
    }
    count++;
    debugf("profiles: added %s (folder %s)\n", p->name, p->folder);
    save();
    return true;
}

void profiles_rename (int profile, const char *name) {
    load();
    if (profile >= 0 && profile < count && name && name[0]) {
        snprintf(list[profile].name, NAME_LENGTH, "%s", name);
        save();
    }
}

bool profiles_delete (int profile) {
    load();
    if (profile <= 0 || profile >= count) {
        return false;   /* the first profile owns the saves the menu always had */
    }
    if (retired_count < RETIRED_MAX) {
        snprintf(retired[retired_count++], NAME_LENGTH, "%s", list[profile].folder);
    }
    debugf("profiles: deleted %s, its saves stay in saves/%s\n", list[profile].name, list[profile].folder);
    memmove(&list[profile], &list[profile + 1], (count - profile - 1) * sizeof(profile_t));
    count--;
    if (current == profile) {
        current = 0;
        generation++;
    } else if (current > profile) {
        current--;
    }
    save();
    return true;
}

bool profiles_save_path (path_t *path, bool create) {
    load();
    if (current == 0) {
        return true;
    }
    path_push_subdir(path, list[current].folder);
    if (create) {
        path_t *folder = path_clone(path);
        path_pop(folder);
        bool error = directory_create(path_get(folder));    /* true when it failed */
        path_free(folder);
        return !error;
    }
    return true;
}

const char *profiles_folder_of (int profile) {
    load();
    return (profile >= 1 && profile < count) ? list[profile].folder : NULL;
}
