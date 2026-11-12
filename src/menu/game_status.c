#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fonts.h"
#include "game_status.h"
#include "menu_features.h"
#include "profiles.h"
#include "safe_file.h"

#define STATUS_PATH      profiles_file("status.txt")   /* the playing profile's */
#define STATUS_TMP_PATH  profiles_file("status.tmp")
#define NAME_LENGTH      (256)

static game_status_list_t list;
static bool loaded = false;
static int loaded_for = 0;      /* profiles_generation() when loaded */
static int generation = 0;

/* "sd:/N64(US)/Game.z64" -> "N64(US)/Game.z64"; other names as they are. */
static const char *from_top (const char *path) {
    const char *colon = strstr(path, ":/");
    return colon ? colon + 2 : path;
}

static void load (void) {
    if (loaded && loaded_for == profiles_generation()) {
        return;
    }
    if (loaded) {
        generation++;   /* another player: their marks */
    }
    loaded = true;
    loaded_for = profiles_generation();
    game_status_list_clear(&list);

    /* If power was cut between remove and rename, the temp file is the good copy. */
    FILE *f = fopen(STATUS_PATH, "r");
    if (!f) {
        f = fopen(STATUS_TMP_PATH, "r");
    }
    if (!f) {
        return;
    }
    char line[NAME_LENGTH + 8];
    char name[NAME_LENGTH];
    if (fgets(line, sizeof(line), f) && strncmp(line, GAME_STATUS_HEADER, strlen(GAME_STATUS_HEADER)) == 0) {
        while (fgets(line, sizeof(line), f)) {
            game_status_t status;
            unsigned day;
            if (game_status_parse(line, &status, name, sizeof(name))) {
                game_status_list_set(&list, name, status);
            } else if (game_status_parse_day(line, &day, name, sizeof(name))) {
                game_status_list_set_day(&list, name, day);     /* after its game's line */
            }
        }
    }
    fclose(f);
    debugf("game status: %d marked (%d playing, %d beaten, %d at 100%%, %d in the backlog)\n", list.count,
        game_status_list_count(&list, GAME_STATUS_PLAYING), game_status_list_count(&list, GAME_STATUS_BEATEN),
        game_status_list_count(&list, GAME_STATUS_COMPLETE), game_status_list_count(&list, GAME_STATUS_BACKLOG));
}

static void save (void) {
    FILE *f = fopen(STATUS_TMP_PATH, "w");
    if (!f) {
        debugf("game status: could not write %s\n", STATUS_TMP_PATH);
        return;
    }
    fprintf(f, "%s\n", GAME_STATUS_HEADER);
    char line[NAME_LENGTH + 8];
    for (int i = 0; i < list.count; i++) {
        game_status_format((game_status_t) list.rows[i].status, list.rows[i].name, line, sizeof(line));
        fputs(line, f);
        game_status_format_day(&list.rows[i], line, sizeof(line));
        fputs(line, f);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(STATUS_TMP_PATH, STATUS_PATH)) {
        debugf("game status: could not replace %s\n", STATUS_PATH);
    }
}

const char *game_status_name (int status) {
    static const char *names[GAME_STATUS_COUNT] = { "None", "Playing", "Beaten", "100%", "Backlog" };
    return (status >= 0 && status < GAME_STATUS_COUNT) ? names[status] : "";
}

int game_status_style (int status) {
    switch (status) {
        case GAME_STATUS_PLAYING: return STL_BLUE;
        case GAME_STATUS_BEATEN: return STL_GREEN;
        case GAME_STATUS_COMPLETE: return STL_YELLOW;
        default: return STL_GRAY;
    }
}

game_status_t game_status_get (const char *path) {
    if (!path || !features_enabled(FEATURE_GAME_STATUS)) {
        return GAME_STATUS_NONE;
    }
    load();
    return game_status_list_get(&list, from_top(path));
}

/* Today as days since 2000-01-01, 0 if the clock isn't set (before 2020): as game_index.c. */
static unsigned today (void) {
    time_t now = time(NULL);
    return (now < 1577836800) ? 0 : (unsigned) ((now - 946684800) / 86400);
}

void game_status_set (const char *path, game_status_t status) {
    if (!path) {
        return;
    }
    load();
    if (game_status_list_get(&list, from_top(path)) == status) {
        return;
    }
    if (!game_status_list_set_on(&list, from_top(path), status, today())) {
        debugf("game status: list full, %s not marked\n", from_top(path));
        return;
    }
    generation++;
    save();
    debugf("game status: %s now %s\n", from_top(path), game_status_name(status));
}

int game_status_beaten_in (int year, const char **names, int most) {
    if (!features_enabled(FEATURE_GAME_STATUS)) {
        return 0;   /* as game_status_get(): the marks are kept but not shown */
    }
    load();
    const game_status_row_t *rows[64];
    int n = game_status_list_beaten_in(&list, year, rows, (most < 64) ? most : 64);
    for (int i = 0; i < n && i < most && i < 64; i++) {
        names[i] = rows[i]->name;
    }
    return n;
}

int game_status_beaten_years (int *years, int most) {
    if (!features_enabled(FEATURE_GAME_STATUS)) {
        return 0;
    }
    load();
    return game_status_list_beaten_years(&list, years, most);
}

int game_status_generation (void) {
    load();     /* also notices a change of player */
    return generation;
}

/* The "Status" list: one row a status, the present one marked. */
static char menu_path[NAME_LENGTH + 8];
static char menu_texts[GAME_STATUS_COUNT][24];
static int menu_current = 0;

static void choose (menu_t *menu, void *arg) {
    (void) menu;
    game_status_set(menu_path, (game_status_t) (intptr_t) arg);
}

static int current_row (menu_t *menu) {
    (void) menu;
    return menu_current;
}

static component_context_menu_t status_menu = {
    .get_default_selection = current_row,
    .list = {
        { .text = menu_texts[0], .action = choose, .arg = (void *) GAME_STATUS_NONE },
        { .text = menu_texts[1], .action = choose, .arg = (void *) GAME_STATUS_PLAYING },
        { .text = menu_texts[2], .action = choose, .arg = (void *) GAME_STATUS_BEATEN },
        { .text = menu_texts[3], .action = choose, .arg = (void *) GAME_STATUS_COMPLETE },
        { .text = menu_texts[4], .action = choose, .arg = (void *) GAME_STATUS_BACKLOG },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

component_context_menu_t *game_status_menu (const char *path) {
    snprintf(menu_path, sizeof(menu_path), "%s", path ? path : "");
    menu_current = path ? (int) game_status_get(path) : 0;
    for (int s = 0; s < GAME_STATUS_COUNT; s++) {
        /* The present one marked with the arrow the font has; the others lined up with it. */
        snprintf(menu_texts[s], sizeof(menu_texts[s]), "%s %s", (s == menu_current) ? "▶" : "  ", game_status_name(s));
    }
    return &status_menu;
}

const char *game_status_row_text (const char *path) {
    static char text[32];
    snprintf(text, sizeof(text), "Status: %s", game_status_name(path ? game_status_get(path) : 0));
    return text;
}

void game_status_rename (const char *old_path, const char *new_path) {
    load();
    if (game_status_list_rename(&list, from_top(old_path), from_top(new_path))) {
        generation++;
        save();
    }
}
