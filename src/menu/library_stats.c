#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fatfs/ff.h>
#include <libdragon.h>

#include "display_name.h"
#include "fonts.h"
#include "game_data.h"
#include "game_index.h"
#include "game_status.h"
#include "games_folders.h"
#include "games_ui.h"
#include "hidden_games.h"
#include "library_stats.h"
#include "menu_features.h"
#include "path.h"
#include "play_stats.h"
#include "screenshot.h"
#include "sound.h"
#include "theme.h"
#include "title_font.h"
#include "ui_components/constants.h"
#include "utils/fs.h"

#define STEP_MS         (6)     /* reading folders stops for the frame after this long */
#define QUEUE_MAX       (64)
#define GENRES_MAX      (32)
#define GENRE_ROWS      (4)     /* the three biggest, then Other */
#define TOP_PLAYED      (5)
#define NAME_LENGTH     (64)

/* Where things are. */
#define TITLE_X         (48)
#define TITLE_Y         (28)
#define AREA_X0         (40)
#define AREA_X1         (600)
#define TILES_Y0        (66)
#define TILES_Y1        (122)
#define TILE_GAP        (10)
#define COLUMNS_Y0      (132)
/* Most played: the big frame on the left, the status counts under it. */
#define PLAYED_X1       (392)
#define PLAYED_Y1       (330)
#define PLAYED_ROW_Y0   (COLUMNS_Y0 + 34)
#define PLAYED_ROW_H    (32)    /* a 28 px line for the 20 px font (smaller boxes drop its text), then the bar */
#define PLAYED_BAR_H    (4)
#define STATUS_Y0       (336)
#define STATUS_Y1       (382)
/* Developers and Genre: two small frames on the right, one above the other. */
#define SMALL_X0        (402)
#define DEVELOPER_Y1    (254)
#define GENRE_Y0        (262)
#define GENRE_Y1        (380)
#define SMALL_ROW_H     (20)
#define BAR_H           (8)
/* A year's page: the big frame and the Beaten frame both run down to here. */
#define YEAR_Y1         (382)
#define YEAR_ROW_H      (40)
#define BEATEN_ROWS     ((YEAR_Y1 - COLUMNS_Y0 - 26) / SMALL_ROW_H)

static bool active = false;

/* The folders still to read. */
static struct {
    char *folder;       /* from the top of the card: "/N64(US)" */
    bool subfolders;    /* an added folder: its folders are read too */
} queue[QUEUE_MAX];
static int queue_count = 0, queue_next = 0;
static const char *added[GAMES_FOLDERS_MAX + 1];
static int added_count = 0;
static path_t *reading = NULL;      /* the folder being read, and where it has got to */
static dir_t entry;
static bool counting = false;
static uint64_t count_began = 0;
static int count_slowest = 0;

/* What was found. */
#define DEVELOPERS_MAX  (64)
#define DEVELOPER_ROWS  (5)
static struct {
    const char *name;   /* the game list's own text, kept while the menu runs */
    int count;
} developers[DEVELOPERS_MAX];
static int developer_count = 0;
static struct {
    const char *name;   /* game_data's own text, kept for good */
    int count;
} genres[GENRES_MAX];
static int genre_count = 0;
static int genre_unknown = 0;
static int games = 0;
static uint64_t bytes = 0;
static int statuses[GAME_STATUS_COUNT];
/* The most played: of all time (page 0) and of each year (Year in Review, a page each). */
typedef struct {
    struct {
        uint32_t seconds;
        char name[NAME_LENGTH];
        uint8_t font;   /* the 20 px title font if the name fits it, else the body font; picked once */
    } rows[TOP_PLAYED];
    int count;
} top_t;
#define YEARS_MAX       (8)
static top_t top;
static top_t year_top[YEARS_MAX];
static int years[YEARS_MAX];        /* newest first: page 1 is years[0] */
static int year_count = 0;
static int page = 0;                /* 0 all time, then a year each */

/* The card's free space: asked once per power-on (the first time it reads the whole FAT). */
static bool free_known = false, free_failed = false;
static uint64_t free_bytes = 0;

static void queue_add (const char *folder, bool subfolders) {
    if (queue_count < QUEUE_MAX && (queue[queue_count].folder = strdup(folder)) != NULL) {
        queue[queue_count++].subfolders = subfolders;
    }
}

static void forget (void) {
    for (int i = 0; i < queue_count; i++) {
        free(queue[i].folder);
    }
    queue_count = queue_next = 0;
    if (reading) {
        path_free(reading);
        reading = NULL;
    }
    counting = false;
}

void library_stats_open (menu_t *menu) {
    forget();
    active = true;
    developer_count = 0;
    memset(statuses, 0, sizeof(statuses));
    genre_count = genre_unknown = games = 0;
    bytes = 0;
    top.count = 0;
    memset(year_top, 0, sizeof(year_top));
    page = 0;

    /* The years: any with play noted (play_years.h), or a game beaten in them. */
    year_count = play_stats_years(years, YEARS_MAX);
    int beaten_years[YEARS_MAX];
    int n = game_status_beaten_years(beaten_years, YEARS_MAX);
    for (int i = 0; i < n; i++) {
        bool seen = false;
        for (int j = 0; j < year_count && !seen; j++) {
            seen = (years[j] == beaten_years[i]);
        }
        if (seen || year_count >= YEARS_MAX) {
            continue;
        }
        int at = year_count++;
        while (at > 0 && years[at - 1] < beaten_years[i]) {
            years[at] = years[at - 1];
            at--;
        }
        years[at] = beaten_years[i];
    }
    added_count = games_folders_list(menu, added);
    for (int i = 0; i < added_count; i++) {
        queue_add(added[i], i > 0);     /* the start folder alone; an added one with its folders */
    }
    counting = true;
    count_began = get_ticks_ms();
    count_slowest = 0;
    sound_play_effect(SFX_ENTER);
}

bool library_stats_active (void) {
    return active;
}

static void note_genre (const char *genre) {
    if (!genre || !genre[0]) {
        genre_unknown++;
        return;
    }
    for (int i = 0; i < genre_count; i++) {
        if (strcmp(genres[i].name, genre) == 0) {
            genres[i].count++;
            return;
        }
    }
    if (genre_count < GENRES_MAX) {
        genres[genre_count].name = genre;
        genres[genre_count++].count = 1;
    } else {
        genre_unknown++;
    }
}

static void note_developer (const char *name) {
    if (!name || !name[0]) {
        return;
    }
    for (int i = 0; i < developer_count; i++) {
        if (developers[i].name == name || strcmp(developers[i].name, name) == 0) {
            developers[i].count++;
            return;
        }
    }
    if (developer_count < DEVELOPERS_MAX) {
        developers[developer_count].name = name;
        developers[developer_count++].count = 1;
    }
}

static void note_played (top_t *t, const char *file_name, uint32_t seconds) {
    if (seconds == 0) {
        return;
    }
    int at;
    if (t->count < TOP_PLAYED) {
        at = t->count++;
    } else if (t->rows[TOP_PLAYED - 1].seconds >= seconds) {
        return;     /* not among the five */
    } else {
        at = TOP_PLAYED - 1;
    }
    while (at > 0 && t->rows[at - 1].seconds < seconds) {
        t->rows[at] = t->rows[at - 1];
        at--;
    }
    t->rows[at].seconds = seconds;
    snprintf(t->rows[at].name, NAME_LENGTH, "%s", display_name_file(file_name));
    t->rows[at].font = 0;
}

static bool is_skipped_folder (const char *name) {
    return name[0] == '.' || strcasecmp(name, "saves") == 0 || strcasecmp(name, "menu") == 0 || strcasecmp(name, "backups") == 0;
}

static void note_entry (menu_t *menu) {
    const char *folder = queue[queue_next].folder;
    if (entry.d_name[0] == '.') {
        return;
    }
    if (entry.d_type == DT_DIR) {
        if (!queue[queue_next].subfolders || is_skipped_folder(entry.d_name)) {
            return;
        }
        char sub[256];
        snprintf(sub, sizeof(sub), "%s/%s", folder, entry.d_name);
        for (int i = 0; i < added_count; i++) {
            if (strcasecmp(added[i], sub) == 0) {
                return;     /* an added folder of its own: read as one */
            }
        }
        queue_add(sub, false);
        return;
    }
    static const char *disk_extensions[] = { "ndd", NULL };
    static const char *rom_extensions[] = { "z64", "n64", "v64", "rom", NULL };
    bool disk = file_has_extensions(entry.d_name, disk_extensions);
    if (!disk && !file_has_extensions(entry.d_name, rom_extensions)) {
        return;
    }
    (void) menu;
    path_push(reading, entry.d_name);
    const char *path = path_get(reading);
    if (!hidden_games_is(path)) {
        games++;
        bytes += (uint64_t) entry.d_size;
        const game_index_entry_t *known = disk ? NULL : game_index_find(path, entry.d_size);
        char code[5] = { 0 };
        if (known) {
            memcpy(code, known->code, 4);
        }
        note_developer(known ? known->developer : NULL);
        const game_data_t *data = (known && !game_index_is_homebrew(code)) ? game_data_find(code) : NULL;
        note_genre(data ? data->genre : NULL);
        game_status_t status = game_status_get(path);
        if (status > GAME_STATUS_NONE && status < GAME_STATUS_COUNT) {
            statuses[status]++;
        }
        note_played(&top, entry.d_name, play_stats_seconds(path));
        for (int y = 0; y < year_count; y++) {
            uint32_t starts, seconds;
            if (play_stats_year_get(path, years[y], &starts, &seconds)) {
                note_played(&year_top[y], entry.d_name, seconds);
            }
        }
    }
    path_pop(reading);
}

/* Read folders for a few milliseconds. */
static void count_step (menu_t *menu) {
    uint64_t began = get_ticks_ms();
    while (counting && (get_ticks_ms() - began) < STEP_MS) {
        int result;
        if (!reading) {
            if (queue_next >= queue_count) {
                counting = false;
                break;
            }
            reading = path_init(menu->storage_prefix, queue[queue_next].folder);
            result = dir_findfirst(path_get(reading), &entry);
        } else {
            result = dir_findnext(path_get(reading), &entry);
        }
        if (result != 0) {
            path_free(reading);
            reading = NULL;
            queue_next++;   /* done with this one (or it is missing) */
            continue;
        }
        note_entry(menu);
    }
    int took = (int) (get_ticks_ms() - began);
    count_slowest = (took > count_slowest) ? took : count_slowest;
    if (!counting) {
        debugf("library stats: %d games in %d folders, %d ms (slowest frame's share %d ms)\n",
            games, queue_count, (int) (get_ticks_ms() - count_began), count_slowest);
        forget();
    }
}

static void ask_free_space (void) {
    uint64_t began = get_ticks_ms();
    DWORD clusters = 0;
    FATFS *fs = NULL;
    if (f_getfree("", &clusters, &fs) == FR_OK && fs) {
        free_bytes = (uint64_t) clusters * fs->csize * 512;
        free_known = true;
    } else {
        free_failed = true;
    }
    debugf("library stats: free space asked in %d ms\n", (int) (get_ticks_ms() - began));
}

void library_stats_process (menu_t *menu) {
    if (menu->actions.back) {
        active = false;
        forget();
        sound_play_effect(SFX_EXIT);
        return;
    }
    joypad_buttons_t pressed = screenshot_pressed();
    int step = (pressed.c_right || menu->actions.go_right) ? 1 : (pressed.c_left || menu->actions.go_left) ? -1 : 0;
    if (step && page + step >= 0 && page + step <= year_count) {
        page += step;   /* All time, then the years, newest first */
        sound_play_effect(SFX_CURSOR);
    }
    if (counting) {
        count_step(menu);
    } else if (!free_known && !free_failed) {
        ask_free_space();   /* after the counting, a frame of its own */
    }
}

/* ---------- drawing ---------- */

static void text (int x, int y, int width, int height, rdpq_align_t align, menu_font_style_t style, const char *string) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = width, .height = height, .align = align, .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES, .style_id = style }, FNT_DEFAULT, x, y, "%s", string);
}

static void size_text (char *out, size_t size, uint64_t n) {
    if (n >= 1000ull * 1000 * 1000) {
        snprintf(out, size, "%d.%d GB", (int) (n / 1000000000ull), (int) ((n / 100000000ull) % 10));
    } else {
        snprintf(out, size, "%d MB", (int) (n / 1000000ull));
    }
}

static void time_text (char *out, size_t size, uint32_t seconds) {
    uint32_t minutes = seconds / 60;
    if (minutes >= 60) {
        snprintf(out, size, "%luh %lum", (unsigned long) (minutes / 60), (unsigned long) (minutes % 60));
    } else {
        snprintf(out, size, "%lum", (unsigned long) minutes);
    }
}

static void tile (int i, const char *number, const char *label) {
    const theme_t *t = theme_get();
    int width = ((AREA_X1 - AREA_X0) - (3 * TILE_GAP)) / 4;
    int x0 = AREA_X0 + i * (width + TILE_GAP);
    games_ui_round_fill(x0, TILES_Y0, x0 + width, TILES_Y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
    rdpq_text_printf(&(rdpq_textparms_t) { .width = width, .height = 34, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .style_id = STL_DEFAULT }, FNT_TITLE, x0, TILES_Y0 + 2, "%s", number);
    text(x0, TILES_Y0 + 34, width, 22, ALIGN_CENTER, STL_GRAY, label);
}

static void frame (int x0, int y0, int x1, int y1) {
    const theme_t *t = theme_get();
    games_ui_round_fill(x0, y0, x1, y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
}

/* A row of a small frame: name, a bar as long as its share of the biggest, the number. */
static void bar_row (int x0, int x1, int y, const char *name, int label_width, int count, int most) {
    const theme_t *t = theme_get();
    int number_width = 30;
    text(x0 + 10, y, label_width, SMALL_ROW_H, ALIGN_LEFT, STL_GRAY, name);
    int bar_x0 = x0 + 10 + label_width + 4;
    int bar_x1 = x1 - 10 - number_width - 4;
    int length = (most > 0) ? ((bar_x1 - bar_x0) * count / most) & ~1 : 0;
    if (length < 2 && count > 0) {
        length = 2;
    }
    rdpq_set_mode_fill(RGBA32(t->accent.r, t->accent.g, t->accent.b, 0xFF));
    if (length > 0) {
        rdpq_fill_rectangle(bar_x0, y + (SMALL_ROW_H - BAR_H) / 2, bar_x0 + length, y + (SMALL_ROW_H + BAR_H) / 2);
    }
    char number[12];
    snprintf(number, sizeof(number), "%d", count);
    text(x1 - 10 - number_width, y, number_width, SMALL_ROW_H, ALIGN_RIGHT, STL_DEFAULT, number);
}

/* Biggest first (a few dozen at most: a plain sort). */
static int biggest_first (int *order, int n, int (*count_of) (int)) {
    for (int i = 0; i < n; i++) {
        order[i] = i;
        for (int j = i; j > 0 && count_of(order[j]) > count_of(order[j - 1]); j--) {
            int k = order[j]; order[j] = order[j - 1]; order[j - 1] = k;
        }
    }
    return n;
}

static int developer_games (int i) {
    return developers[i].count;
}

static void draw_developers (void) {
    int x0 = SMALL_X0, x1 = AREA_X1, y0 = COLUMNS_Y0;
    frame(x0, y0, x1, DEVELOPER_Y1);
    text(x0 + 10, y0 + 2, x1 - x0 - 20, 22, ALIGN_LEFT, STL_DEFAULT, "Developers");
    int order[DEVELOPERS_MAX];
    biggest_first(order, developer_count, developer_games);
    for (int i = 0; i < developer_count && i < DEVELOPER_ROWS; i++) {
        bar_row(x0, x1, y0 + 22 + i * SMALL_ROW_H, developers[order[i]].name, 90, developers[order[i]].count, developers[order[0]].count);
    }
}

static int genre_games (int i) {
    return genres[i].count;
}

static void draw_genres (void) {
    int x0 = SMALL_X0, x1 = AREA_X1, y0 = GENRE_Y0;
    frame(x0, y0, x1, GENRE_Y1);
    text(x0 + 10, y0 + 2, x1 - x0 - 20, 22, ALIGN_LEFT, STL_DEFAULT, "Genre");
    int order[GENRES_MAX];
    biggest_first(order, genre_count, genre_games);
    bool other = (genre_count > GENRE_ROWS) || (genre_unknown > 0);
    int shown = other ? ((genre_count < GENRE_ROWS - 1) ? genre_count : GENRE_ROWS - 1) : genre_count;
    int rest = genre_unknown;
    for (int i = shown; i < genre_count; i++) {
        rest += genres[order[i]].count;
    }
    int most = rest;
    for (int i = 0; i < shown; i++) {
        most = (genres[order[i]].count > most) ? genres[order[i]].count : most;
    }
    for (int i = 0; i < shown; i++) {
        bar_row(x0, x1, y0 + 22 + i * SMALL_ROW_H, genres[order[i]].name, 80, genres[order[i]].count, most);
    }
    if (rest > 0) {
        bar_row(x0, x1, y0 + 22 + shown * SMALL_ROW_H, "Other", 80, rest, most);
    }
}

/* The big frame: rank, name in the 20 px title font when it fits, time, and a bar of its share of the first. */
static void draw_played (top_t *t, const char *heading, int y1, int row_h) {
    const theme_t *theme = theme_get();
    int x0 = AREA_X0, x1 = PLAYED_X1;
    frame(x0, COLUMNS_Y0, x1, y1);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = x1 - x0 - 24, .height = 30, .valign = VALIGN_CENTER, .style_id = STL_DEFAULT },
        FNT_TITLE_MEDIUM, x0 + 12, COLUMNS_Y0 + 4, "%s", heading);
    if (t->count == 0) {
        text(x0 + 12, PLAYED_ROW_Y0, x1 - x0 - 24, row_h, ALIGN_LEFT, STL_GRAY, "Nothing played yet.");
        return;
    }
    int rank_w = 22, time_w = 72;
    int name_x = x0 + 12 + rank_w;
    int name_w = x1 - 12 - time_w - 8 - name_x;
    int line_h = PLAYED_ROW_H - PLAYED_BAR_H;
    uint32_t first = (t->rows[0].seconds / 60) ? (t->rows[0].seconds / 60) : 1;
    for (int i = 0; i < t->count; i++) {
        int y = PLAYED_ROW_Y0 + i * row_h;
        if (!t->rows[i].font) {
            uint8_t pick = title_font_pick(t->rows[i].name, name_w);
            t->rows[i].font = (pick == FNT_TITLE) ? FNT_TITLE_MEDIUM : pick;     /* at most 20 px: five long names */
        }
        char rank[4], hours[16];
        snprintf(rank, sizeof(rank), "%d", i + 1);
        time_text(hours, sizeof(hours), t->rows[i].seconds);
        text(x0 + 12, y, rank_w, line_h, ALIGN_LEFT, STL_GRAY, rank);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = name_w, .height = line_h, .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT }, t->rows[i].font, name_x, y, "%s", t->rows[i].name);
        text(x1 - 12 - time_w, y, time_w, line_h, ALIGN_RIGHT, STL_DEFAULT, hours);
        int bar_x1 = x1 - 12;
        int length = ((bar_x1 - name_x) * (int) (t->rows[i].seconds / 60) / (int) first) & ~1;
        length = (length < 2) ? 2 : (length > bar_x1 - name_x) ? (bar_x1 - name_x) : length;
        rdpq_set_mode_fill(RGBA32(theme->accent.r, theme->accent.g, theme->accent.b, 0xFF));
        rdpq_fill_rectangle(name_x, y + line_h, name_x + length, y + line_h + PLAYED_BAR_H);
    }
}

/* A year's games beaten, by name, in the right-hand frame. */
static void draw_beaten (int year) {
    int x0 = SMALL_X0, x1 = AREA_X1, y0 = COLUMNS_Y0, y1 = YEAR_Y1;
    frame(x0, y0, x1, y1);
    char heading[32];
    snprintf(heading, sizeof(heading), "Beaten in %d", year);
    text(x0 + 10, y0 + 2, x1 - x0 - 20, 22, ALIGN_LEFT, STL_DEFAULT, heading);
    const char *names[BEATEN_ROWS];
    int n = game_status_beaten_in(year, names, BEATEN_ROWS);
    if (n == 0) {
        text(x0 + 10, y0 + 22, x1 - x0 - 20, SMALL_ROW_H, ALIGN_LEFT, STL_GRAY, "None marked.");
        return;
    }
    int shown = (n > BEATEN_ROWS) ? BEATEN_ROWS - 1 : n;    /* the last row says how many more */
    for (int i = 0; i < shown; i++) {
        const char *slash = strrchr(names[i], '/');
        text(x0 + 10, y0 + 22 + i * SMALL_ROW_H, x1 - x0 - 20, SMALL_ROW_H, ALIGN_LEFT, STL_GRAY,
            display_name_file(slash ? slash + 1 : names[i]));
    }
    if (n > shown) {
        char more[24];
        snprintf(more, sizeof(more), "and %d more", n - shown);
        text(x0 + 10, y0 + 22 + shown * SMALL_ROW_H, x1 - x0 - 20, SMALL_ROW_H, ALIGN_LEFT, STL_DEFAULT, more);
    }
}

static void draw_statuses (void) {
    if (!features_enabled(FEATURE_GAME_STATUS)) {
        return;
    }
    frame(AREA_X0, STATUS_Y0, PLAYED_X1, STATUS_Y1);
    static const game_status_t order[] = { GAME_STATUS_PLAYING, GAME_STATUS_BACKLOG, GAME_STATUS_BEATEN, GAME_STATUS_COMPLETE };
    /* Two by two: four side by side didn't fit their numbers. */
    int width = (PLAYED_X1 - AREA_X0 - 8) / 2;
    for (int i = 0; i < 4; i++) {
        char line[32];
        snprintf(line, sizeof(line), "%s %d", game_status_name(order[i]), statuses[order[i]]);
        text(AREA_X0 + 4 + (i % 2) * width, STATUS_Y0 + 3 + (i / 2) * SMALL_ROW_H, width, SMALL_ROW_H, ALIGN_CENTER,
            game_status_style(order[i]), line);
    }
}

/* "+3h", "-45m" (time) or "+3", "-2", after a tile's word, in green or orange; "" when the same. */
static void change_text (char *out, size_t size, const char *word, int64_t now, int64_t before, bool time) {
    int64_t change = now - before;
    if (change == 0) {
        snprintf(out, size, "%s", word);
        return;
    }
    char amount[16];
    int64_t magnitude = (change < 0) ? -change : change;
    if (time) {
        if (magnitude >= 3600) {
            snprintf(amount, sizeof(amount), "%dh", (int) (magnitude / 3600));
        } else {
            snprintf(amount, sizeof(amount), "%dm", (int) (magnitude / 60));
        }
    } else {
        snprintf(amount, sizeof(amount), "%d", (int) magnitude);
    }
    snprintf(out, size, "%s ^%02X%c%s", word, (change > 0) ? STL_GREEN : STL_ORANGE, (change > 0) ? '+' : '-', amount);
}

/* A year's tiles: time played, games started, starts and games beaten, each against the year before when it has any. */
static void year_tiles (int year) {
    uint32_t seconds, starts, before_seconds, before_starts;
    int played_games, before_games;
    play_stats_year_totals(year, &seconds, &starts, &played_games);
    play_stats_year_totals(year - 1, &before_seconds, &before_starts, &before_games);
    const char *names[1];
    int beaten = game_status_beaten_in(year, names, 1);
    int before_beaten = game_status_beaten_in(year - 1, names, 1);
    bool compare = before_seconds > 0 || before_starts > 0 || before_beaten > 0;

    char number[24], label[48];
    time_text(number, sizeof(number), seconds);
    change_text(label, sizeof(label), "played", seconds, compare ? before_seconds : seconds, true);
    tile(0, number, label);
    snprintf(number, sizeof(number), "%d", played_games);
    change_text(label, sizeof(label), (played_games == 1) ? "game" : "games", played_games, compare ? before_games : played_games, false);
    tile(1, number, label);
    snprintf(number, sizeof(number), "%lu", (unsigned long) starts);
    change_text(label, sizeof(label), "starts", starts, compare ? before_starts : starts, false);
    tile(2, number, label);
    snprintf(number, sizeof(number), "%d", beaten);
    change_text(label, sizeof(label), "beaten", beaten, compare ? before_beaten : beaten, false);
    tile(3, number, label);
}

static void all_time_tiles (void) {
    char number[24], size[24], played[24], free_space[24];
    snprintf(number, sizeof(number), "%d", games);
    size_text(size, sizeof(size), bytes);
    uint32_t seconds = 0;
    int played_games = 0;
    play_stats_totals(&seconds, &played_games);
    time_text(played, sizeof(played), seconds);
    if (free_known) {
        size_text(free_space, sizeof(free_space), free_bytes);
    } else {
        snprintf(free_space, sizeof(free_space), "%s", free_failed ? "?" : "...");
    }
    tile(0, number, (games == 1) ? "game" : "games");
    tile(1, size, "in all");
    tile(2, played, "played");
    tile(3, free_space, "free on SD");
}

void library_stats_draw (void) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = 360, .height = 34, .valign = VALIGN_CENTER },
        FNT_TITLE, TITLE_X, TITLE_Y, (page == 0) ? "Library Stats" : "Year in Review");

    /* Which page, with arrows where there is another. */
    if (year_count > 0) {
        char which[32], when[12];
        snprintf(when, sizeof(when), "%d", (page > 0) ? years[page - 1] : 0);
        snprintf(which, sizeof(which), "%s  %s  %s", (page > 0) ? "\u25C0" : " ", (page > 0) ? when : "All time",
            (page < year_count) ? "\u25B6" : " ");
        text(PLAYED_X1, TITLE_Y, AREA_X1 - PLAYED_X1, 34, ALIGN_RIGHT, STL_DEFAULT, which);
    }

    int year = (page > 0) ? years[page - 1] : 0;
    if (page == 0) {
        all_time_tiles();
    } else {
        year_tiles(year);
    }

    if (counting) {
        text(AREA_X0, COLUMNS_Y0, AREA_X1 - AREA_X0, GENRE_Y1 - COLUMNS_Y0, ALIGN_CENTER, STL_GRAY, "Counting your games...");
    } else if (page == 0) {
        draw_played(&top, "Most Played", PLAYED_Y1, PLAYED_ROW_H);
        draw_developers();
        draw_genres();
        draw_statuses();
    } else {
        char heading[32];
        snprintf(heading, sizeof(heading), "Most Played in %d", year);
        draw_played(&year_top[page - 1], heading, YEAR_Y1, YEAR_ROW_H);
        draw_beaten(year);
    }

    games_ui_hints_backdrop_draw();
    int x = games_ui_hint_draw(GAMES_UI_HINTS_X, 1, "B", "Back");
    if (year_count > 0) {
        games_ui_hint_draw(x, 1, "C", "\u25C0\u25B6 Year");
    }
}
