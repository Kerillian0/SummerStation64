/**
 * @file dd_pair.c
 * @brief 64DD pairing: a disk and the cartridge it is played with.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "dd_pair.h"
#include "disk_info.h"
#include "display_name.h"
#include "fonts.h"
#include "game_index.h"
#include "games_filter.h"
#include "safe_file.h"
#include "sound.h"
#include "ui_components.h"
#include "ui_components/constants.h"

#define FILE_PATH       "sd:/menu/ddpairs.txt"
#define FILE_TMP_PATH   "sd:/menu/ddpairs.tmp"
#define FILE_HEADER     "ddpairs 1"
#define PAIRS_MAX       (32)
#define IDS_MAX         (64)
#define NAME_LENGTH     (200)
#define SETTLE_MS       (350)

/* ---------- the player's own pairs ---------- */

static struct {
    char disk[NAME_LENGTH];
    char cart[NAME_LENGTH];     /* "-" for none */
} pairs[PAIRS_MAX];
static int pairs_count = 0;
static bool loaded = false;

/* "sd:/64DD/Game.ndd" -> "64DD/Game.ndd". */
static const char *from_top (const char *path) {
    const char *colon = strstr(path, ":/");
    return colon ? colon + 2 : path;
}

static void load (void) {
    if (loaded) {
        return;
    }
    loaded = true;
    FILE *f = fopen(FILE_PATH, "r");
    if (!f) {
        f = fopen(FILE_TMP_PATH, "r");  /* a save that was cut off */
    }
    if (!f) {
        return;
    }
    char line[2 * NAME_LENGTH + 4];
    if (fgets(line, sizeof(line), f) && strncmp(line, FILE_HEADER, strlen(FILE_HEADER)) == 0) {
        while (pairs_count < PAIRS_MAX && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *tab = strchr(line, '\t');
            if (!tab || line[0] == '#') {
                continue;
            }
            *tab = '\0';
            snprintf(pairs[pairs_count].disk, NAME_LENGTH, "%s", line);
            snprintf(pairs[pairs_count].cart, NAME_LENGTH, "%s", tab + 1);
            pairs_count++;
        }
    }
    fclose(f);
    debugf("dd pairs: %d chosen\n", pairs_count);
}

static void save (void) {
    FILE *f = fopen(FILE_TMP_PATH, "w");
    if (!f) {
        debugf("dd pairs: could not write %s\n", FILE_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", FILE_HEADER);
    for (int i = 0; i < pairs_count; i++) {
        fprintf(f, "%s\t%s\n", pairs[i].disk, pairs[i].cart);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(FILE_TMP_PATH, FILE_PATH)) {
        debugf("dd pairs: could not replace %s\n", FILE_PATH);
    }
}

static int find_pair (const char *disk) {
    load();
    for (int i = 0; i < pairs_count; i++) {
        if (strcmp(pairs[i].disk, disk) == 0) {
            return i;
        }
    }
    return -1;
}

/* NULL: back to the rule. */
static void set_pair (const char *disk, const char *cart) {
    int i = find_pair(disk);
    if (!cart) {
        if (i >= 0) {
            pairs[i] = pairs[--pairs_count];
            save();
        }
        return;
    }
    if (i < 0) {
        if (pairs_count >= PAIRS_MAX) {
            return;
        }
        i = pairs_count++;
        snprintf(pairs[i].disk, NAME_LENGTH, "%s", disk);
    }
    snprintf(pairs[i].cart, NAME_LENGTH, "%s", cart);
    save();
}

/* ---------- disk IDs, read once each while the console is on ---------- */

static struct {
    uint32_t key;
    char id[4];
} ids[IDS_MAX];
static int ids_count = 0;

static uint32_t key_of (const char *name) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) name; *c; c++) {
        h = (h ^ *c) * 16777619u;
    }
    return h;
}

static const char *known_id (const char *disk) {
    uint32_t key = key_of(disk);
    for (int i = 0; i < ids_count; i++) {
        if (ids[i].key == key) {
            return ids[i].id;
        }
    }
    return NULL;
}

static void note_id (const char *disk, const char id[4]) {
    if (known_id(disk)) {
        return;
    }
    int i = (ids_count < IDS_MAX) ? ids_count++ : (int) (key_of(disk) % IDS_MAX);
    ids[i].key = key_of(disk);
    memcpy(ids[i].id, id, 4);
}

/* ---------- finding the cartridge ---------- */

/* The rule: a cartridge whose code ends like the disk's ID. */
static bool rule_matches (const char id[4], const char code[4]) {
    if (id[0] == 'D' && memcmp(id, "DEZA", 4) == 0) {
        return code[1] == 'D' && code[2] == 'Z' && code[3] == 'J';     /* Dezaemon 3D's prototype disk */
    }
    return id[0] >= 'E' && id[0] <= 'Z' && memcmp(id + 1, code + 1, 3) == 0;
}

/* The Games list's entry for a name from the top of the card. */
static const entry_t *entry_named (menu_t *menu, const char *name) {
    int all = games_filter_all_entries(menu->browser.entries);
    for (int i = 0; menu->browser.list && i < all; i++) {
        if (strcmp(menu->browser.list[i].name, name) == 0) {
            return &menu->browser.list[i];
        }
    }
    return NULL;
}

static const entry_t *rule_cart (menu_t *menu, const char *id);

/* The disk's cartridge in the Games list, or NULL. `chosen` says whether the player chose it (or chose none). */
static const entry_t *find_cart (menu_t *menu, const char *disk, const char *id, bool *chosen) {
    int p = find_pair(disk);
    *chosen = (p >= 0);
    if (p >= 0) {
        return strcmp(pairs[p].cart, "-") ? entry_named(menu, pairs[p].cart) : NULL;
    }
    return rule_cart(menu, id);
}

/* The cartridge the rule picks for this ID in the Games list, or NULL. */
static const entry_t *rule_cart (menu_t *menu, const char *id) {
    if (!id || !menu->browser.list) {
        return NULL;
    }
    const entry_t *best = NULL;
    int best_score = -1;
    int all = games_filter_all_entries(menu->browser.entries);
    for (int i = 0; i < all; i++) {
        const entry_t *e = &menu->browser.list[i];
        if (e->type != ENTRY_TYPE_ROM) {
            continue;
        }
        path_t *path = path_clone_push(menu->browser.directory, e->name);
        const game_index_entry_t *known = game_index_find(path_get(path), e->size);
        path_free(path);
        if (!known || !rule_matches(id, known->code)) {
            continue;
        }
        int score = ((known->code[0] == 'C') ? 2 : 0) + (games_filter_is_hack(e) ? 0 : 1);
        if (score > best_score) {
            best = e;
            best_score = score;
        }
    }
    return best;
}

const char *dd_pair_needs (const char id[4]) {
    /* The cartridges of the expansion disks upstream lists (docs/17_64dd.md);
       of these only F-Zero X's Expansion Kit was sold. */
    static const struct { const char *letters, *game; } games[] = {
        { "FZ", "F-Zero X" },
        { "LB", "Mario Party" },
        { "ZL", "The Legend of Zelda: Ocarina of Time" },
        { "PS", "Pokemon Stadium" },
        { "P2", "Pokemon Stadium 2" },
        { "DZ", "Dezaemon 3D" },
    };
    bool expansion = (id[0] >= 'E' && id[0] <= 'Z') || memcmp(id, "DEZA", 4) == 0;
    if (!expansion) {
        return NULL;
    }
    const char *letters = (memcmp(id, "DEZA", 4) == 0) ? "DZ" : id + 1;
    for (int i = 0; i < (int) (sizeof(games) / sizeof(games[0])); i++) {
        if (memcmp(letters, games[i].letters, 2) == 0) {
            return games[i].game;
        }
    }
    return "";      /* an expansion disk for a game not listed here */
}

path_t *dd_pair_cart (menu_t *menu, const char *disk_path, const char id[4]) {
    const char *disk = from_top(disk_path);
    note_id(disk, id);
    bool chosen;
    const entry_t *cart = find_cart(menu, disk, known_id(disk), &chosen);
    if (!cart) {
        debugf("dd pairs: %.4s %s\n", id, chosen ? "played alone (the player's choice)" : "has no cartridge");
        return NULL;
    }
    debugf("dd pairs: %.4s with %s%s\n", id, cart->name, chosen ? " (the player's choice)" : "");
    return path_clone_push(menu->browser.directory, cart->name);
}

/* The panel asks every frame: worked out once per selection. */
static const entry_t *text_for = NULL;
static bool text_known = false;
static char text[96];

const char *dd_pair_with_text (menu_t *menu, const entry_t *disk) {
    if (!disk || disk->type != ENTRY_TYPE_DISK) {
        return NULL;
    }
    if (disk != text_for || !text_known) {
        text_for = disk;
        text_known = false;
        const char *id = known_id(disk->name);
        bool chosen;
        const entry_t *cart = find_cart(menu, disk->name, id, &chosen);
        if (!id && !chosen) {
            return NULL;    /* not read yet (dd_pair_after_draw) */
        }
        text_known = true;
        text[0] = '\0';
        if (cart) {
            snprintf(text, sizeof(text), "With %s", display_name(cart));
        }
    }
    return text[0] ? text : NULL;
}

static const entry_t *watched = NULL;
static uint64_t watched_since = 0;

void dd_pair_after_draw (menu_t *menu, const entry_t *selected) {
    if (!selected || selected->type != ENTRY_TYPE_DISK) {
        watched = NULL;
        return;
    }
    if (selected != watched) {
        watched = selected;
        watched_since = get_ticks_ms();
        return;
    }
    if (known_id(selected->name) || (get_ticks_ms() - watched_since) < SETTLE_MS) {
        return;
    }
    path_t *path = path_clone_push(menu->browser.directory, selected->name);
    static disk_info_t info;
    uint64_t began = get_ticks_ms();
    disk_err_t err = disk_info_load(path, &info);
    path_free(path);
    note_id(selected->name, (err == DISK_OK) ? info.id : "----");
    text_known = false;
    debugf("dd pairs: read %s in %d ms: %.4s\n", selected->name, (int) (get_ticks_ms() - began), (err == DISK_OK) ? info.id : "----");
}

/* ---------- choosing by hand ---------- */

#define ROWS_SHOWN      (9)
#define ROW_HEIGHT      (26)
#define LIST_WIDTH      (440)
#define CARTS_MAX       (512)

static bool showing = false;
static bool changed = false;        /* a choice was made since dd_pair_take_change() last asked */
static char picking[NAME_LENGTH];   /* the disk, from the top of the card */
static const entry_t **carts = NULL;    /* the cartridges: those the rule matches first, then the rest by name */
static int carts_count = 0;
static int suggested = 0;           /* how many of carts the rule matches */
static const entry_t *automatic = NULL;     /* what Automatic plays it with */
static int row = 0, first_row = 0;  /* 0 Automatic, the suggested, No cartridge, then the others */
static int current_row = 0;         /* the row in use now */

/* Row r's cartridge, or NULL for Automatic (r 0) and No cartridge. */
static const entry_t *cart_at (int r) {
    if (r <= 0 || r == suggested + 1) {
        return NULL;
    }
    return carts[(r <= suggested) ? r - 1 : r - 2];
}

static int row_of_none (void) {
    return suggested + 1;
}

void dd_pair_pick (menu_t *menu, const char *disk_path) {
    snprintf(picking, sizeof(picking), "%s", from_top(disk_path));
    free(carts);
    carts_count = 0;
    suggested = 0;
    const char *id = known_id(picking);
    automatic = rule_cart(menu, id);
    int all = games_filter_all_entries(menu->browser.entries);
    carts = malloc(((all < CARTS_MAX) ? all : CARTS_MAX) * sizeof(entry_t *) + 1);
    bool *matched = malloc(((all < CARTS_MAX) ? all : CARTS_MAX) + 1);
    for (int i = 0; carts && matched && i < all && carts_count < CARTS_MAX; i++) {
        const entry_t *e = &menu->browser.list[i];
        if (e->type != ENTRY_TYPE_ROM) {
            continue;
        }
        bool match = false;
        if (id) {
            path_t *path = path_clone_push(menu->browser.directory, e->name);
            const game_index_entry_t *known = game_index_find(path_get(path), e->size);
            path_free(path);
            match = known && rule_matches(id, known->code);
        }
        matched[carts_count] = match;
        carts[carts_count++] = e;
        suggested += match ? 1 : 0;
    }
    /* Matching ones first, each part by name. display_name() keeps one name
       at a time, so names are compared two copies at a time. */
    for (int i = 1; carts && matched && i < carts_count; i++) {
        const entry_t *e = carts[i];
        bool m = matched[i];
        char name[128];
        snprintf(name, sizeof(name), "%s", display_name(e));
        int j = i - 1;
        while (j >= 0 && (matched[j] < m || (matched[j] == m && strcasecmp(display_name(carts[j]), name) > 0))) {
            carts[j + 1] = carts[j];
            matched[j + 1] = matched[j];
            j--;
        }
        carts[j + 1] = e;
        matched[j + 1] = m;
    }
    free(matched);
    if (!carts) {
        carts_count = suggested = 0;
    }
    int p = find_pair(picking);
    current_row = 0;
    if (p >= 0) {
        current_row = row_of_none();
        for (int r = 1; strcmp(pairs[p].cart, "-") && r < 2 + carts_count; r++) {
            if (cart_at(r) && strcmp(cart_at(r)->name, pairs[p].cart) == 0) {
                current_row = r;
            }
        }
    }
    row = current_row;
    first_row = (row >= ROWS_SHOWN) ? row - (ROWS_SHOWN / 2) : 0;
    showing = true;
}

bool dd_pair_alone (const char *disk_path) {
    int p = find_pair(from_top(disk_path));
    return p >= 0 && strcmp(pairs[p].cart, "-") == 0;
}

bool dd_pair_take_change (void) {
    bool was = changed;
    changed = false;
    return was;
}

static void close_pick (void) {
    showing = false;
    free(carts);
    carts = NULL;
    carts_count = 0;
}

bool dd_pair_process (menu_t *menu) {
    if (!showing) {
        return false;
    }
    int rows = 2 + carts_count;
    int step = menu->actions.go_fast ? ROWS_SHOWN : 1;
    if (menu->actions.go_up) {
        row = (row - step < 0) ? 0 : row - step;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down) {
        row = (row + step >= rows) ? rows - 1 : row + step;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.back) {
        close_pick();
        sound_play_effect(SFX_EXIT);
        return true;
    } else if (menu->actions.enter) {
        set_pair(picking, (row == 0) ? NULL : cart_at(row) ? cart_at(row)->name : "-");
        text_known = false;
        changed = true;
        close_pick();
        sound_play_effect(SFX_SETTING);
        return true;
    }
    if (row < first_row) {
        first_row = row;
    } else if (row >= first_row + ROWS_SHOWN) {
        first_row = row - ROWS_SHOWN + 1;
    }
    return true;
}

void dd_pair_draw (void) {
    if (!showing) {
        return;
    }
    int rows = 2 + carts_count;
    int shown = (rows < ROWS_SHOWN) ? rows : ROWS_SHOWN;
    int height = 40 + (shown * ROW_HEIGHT);
    ui_components_dialog_draw(LIST_WIDTH, height);
    int x0 = DISPLAY_CENTER_X - (LIST_WIDTH / 2);
    int y0 = DISPLAY_CENTER_Y - (height / 2);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH, .height = 26, .align = ALIGN_CENTER, .valign = VALIGN_CENTER, .style_id = STL_GRAY },
        FNT_DEFAULT, x0, y0 + 6, "Play this disk with");
    for (int i = 0; i < shown; i++) {
        int r = first_row + i;
        int y = y0 + 34 + (i * ROW_HEIGHT);
        if (r == row) {
            ui_components_box_draw(x0 + 8, y, x0 + LIST_WIDTH - 8, y + ROW_HEIGHT, CONTEXT_MENU_HIGHLIGHT_COLOR);
        }
        char automatic_label[160];
        if (r == 0) {
            snprintf(automatic_label, sizeof(automatic_label), "Automatic: %s", automatic ? display_name(automatic) : "none found");
        }
        const char *label = (r == 0) ? automatic_label : cart_at(r) ? display_name(cart_at(r)) : "No cartridge (disk alone)";
        rdpq_text_printf(&(rdpq_textparms_t) { .width = LIST_WIDTH - 80, .height = ROW_HEIGHT, .valign = VALIGN_CENTER, .wrap = WRAP_ELLIPSES,
            .style_id = (r == current_row) ? STL_GREEN : (r >= 1 && r <= suggested) ? STL_BLUE : STL_DEFAULT }, FNT_DEFAULT, x0 + 24, y, "%s", label);
    }
    if (first_row > 0) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT, x0 + LIST_WIDTH - 32, y0 + 34, "▲");
    }
    if (first_row + shown < rows) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT, x0 + LIST_WIDTH - 32, y0 + 34 + ((shown - 1) * ROW_HEIGHT), "▼");
    }
}
