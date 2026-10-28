#include <ctype.h>
#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "games_filter.h"
#include "menu_options.h"
#include "region_pick.h"
#include "profiles.h"
#include "safe_file.h"

#define PICKS_PATH  profiles_file("regionpicks.txt")   /* the playing profile's */
#define PICKS_TMP_PATH  profiles_file("regionpicks.tmp")

/* 6 bytes a game. When full, the game changed longest ago makes way. */
#define MAX_PICKS       (64)

/* Starts of one other region's copy in a row that make it the game's choice. */
#define STARTS_TO_PICK  (3)

typedef struct {
    char game[3];       /* the code's first three letters */
    char last;          /* region of the copy last started: 'U', 'J', 'E' */
    uint8_t streak;     /* starts of that region in a row */
    char chosen;        /* the game's own choice, or 0 */
} pick_t;

static pick_t picks[MAX_PICKS];
static int pick_count = 0;
static bool loaded = false;
static int loaded_for = 0;     /* profiles_generation() when loaded */

const char *region_pick_copies_name (int choice) {
    static const char *names[REGION_COPIES_COUNT] = { "Separate", "One Cover" };
    return (choice >= 0 && choice < REGION_COPIES_COUNT) ? names[choice] : "";
}

const char *region_pick_preferred_name (int choice) {
    static const char *names[PREFERRED_COUNT] = { "USA", "Japan", "Europe" };
    return (choice >= 0 && choice < PREFERRED_COUNT) ? names[choice] : "";
}

/* 'U' USA, 'J' Japan, 'E' Europe (PAL), 'A' every region, 0 not known;
   the same letters the region filter goes by. */
static char region_of (char letter) {
    switch (toupper((unsigned char) letter)) {
        case 'E': case 'N': return 'U';
        case 'J': return 'J';
        case 'P': case 'D': case 'F': case 'I': case 'S': case 'U':
        case 'X': case 'Y': case 'H': case 'W': case 'L':
            return 'E';
        case 'A': return 'A';
        default: return 0;
    }
}

static char preferred (void) {
    static const char letters[PREFERRED_COUNT] = { 'U', 'J', 'E' };
    int p = options_get(OPTION_PREFERRED_REGION);
    return (p >= 0 && p < PREFERRED_COUNT) ? letters[p] : 'U';
}

/* One game per line: its three letters, the region last started, how many
   in a row, and its own choice ('-' for none). */
static void load (void) {
    if (loaded && loaded_for == profiles_generation()) {
        return;
    }
    loaded = true;
    loaded_for = profiles_generation();   /* another player: forget the last one's */
    pick_count = 0;

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(PICKS_PATH, "r");
    if (!f) {
        f = fopen(PICKS_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }
    char line[32];
    while (fgets(line, sizeof(line), f) && pick_count < MAX_PICKS) {
        char game[4], last, chosen;
        int streak;
        if (sscanf(line, "%3s %c %d %c", game, &last, &streak, &chosen) == 4 && strlen(game) == 3) {
            pick_t *p = &picks[pick_count++];
            memcpy(p->game, game, 3);
            p->last = last;
            p->streak = (uint8_t) streak;
            p->chosen = (chosen == '-') ? 0 : chosen;
        }
    }
    fclose(f);
}

/* Written to a temporary file first, so a power cut can't leave half a file. */
static void save (void) {
    FILE *f = fopen(PICKS_TMP_PATH, "w");
    if (!f) {
        debugf("region pick: could not write %s\n", PICKS_TMP_PATH);
        return;
    }
    for (int i = 0; i < pick_count; i++) {
        fprintf(f, "%.3s %c %d %c\n", picks[i].game, picks[i].last, picks[i].streak, picks[i].chosen ? picks[i].chosen : '-');
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(PICKS_TMP_PATH, PICKS_PATH)) {
        debugf("region pick: could not replace %s\n", PICKS_PATH);
    }
}

static pick_t *find (const char *code) {
    for (int i = 0; i < pick_count; i++) {
        if (memcmp(picks[i].game, code, 3) == 0) {
            return &picks[i];
        }
    }
    return NULL;
}

static void drop (pick_t *p) {
    int at = (int) (p - picks);
    memmove(&picks[at], &picks[at + 1], (pick_count - at - 1) * sizeof(pick_t));
    pick_count--;
}

int region_pick_score (const char *code) {
    char r = region_of(code[3]);
    if (!r) {
        return 0;
    }
    load();
    pick_t *p = find(code);
    if (p && p->chosen && (r == p->chosen || r == 'A')) {
        return 2;
    }
    return (r == preferred() || r == 'A') ? 1 : 0;
}

int region_pick_list_order (const char *code) {
    switch (code ? region_of(code[3]) : 0) {
        case 'U': case 'A': return 0;
        case 'E': return 1;
        case 'J': return 2;
        default: return 3;
    }
}

void region_pick_launched (menu_t *menu, const char *rom_path, const char *code) {
    if (!code || options_get(OPTION_REGION_COPIES) != REGION_COPIES_ONE_COVER) {
        return;
    }
    char r = region_of(code[3]);
    if (!r || !games_filter_region_copies(menu, rom_path)) {
        return;
    }
    load();
    pick_t *p = find(code);

    if (r == preferred() || r == 'A') {
        if (p) {
            debugf("region pick: %.3s back to the preferred region\n", code);
            drop(p);
            save();
        }
        return;
    }
    if (p && p->chosen == r) {
        return;     /* already this game's choice */
    }
    if (!p) {
        if (pick_count == MAX_PICKS) {
            drop(&picks[0]);
        }
        p = &picks[pick_count++];
        memcpy(p->game, code, 3);
        p->chosen = 0;
        p->last = 0;
        p->streak = 0;
    } else {
        /* Changed last: to the end, so the oldest stay first to make way. */
        pick_t keep = *p;
        drop(p);
        p = &picks[pick_count++];
        *p = keep;
    }
    if (p->last == r) {
        p->streak++;
    } else {
        p->last = r;
        p->streak = 1;
    }
    if (p->streak >= STARTS_TO_PICK) {
        p->chosen = r;
        debugf("region pick: %.3s now shows its %c copy\n", code, r);
    } else {
        debugf("region pick: %.3s, %c copy started %d of %d in a row\n", code, r, p->streak, STARTS_TO_PICK);
    }
    save();
}

void region_pick_forget_all (void) {
    load();
    if (pick_count > 0) {
        pick_count = 0;
        save();
        debugf("region pick: every game back to the preferred region\n");
    }
}
