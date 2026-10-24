#include <libdragon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art_report.h"
#include "baked_art.h"
#include "baked_meta.h"
#include "game_index.h"
#include "games_filter.h"
#include "games_folders.h"
#include "safe_file.h"
#include "utils/fs.h"

#define REPORT_PATH     "sd:/menu/missingart.txt"
#define REPORT_TMP_PATH "sd:/menu/missingart.tmp"

/* More games than this on the Games tab are left out of one report. */
#define MAX_GAMES       (1024)

static const char *rom_extensions[] = { "z64", "n64", "v64", "rom", NULL };

typedef struct {
    char *name;         /* from the top of the card, "N64(US)/Game.z64" */
    int64_t size;
} game_t;

static game_t *games = NULL;
static int game_count = 0;
static int next = 0;            /* the game to check next */
static int lacking = 0;         /* games that lack something */
static FILE *out = NULL;
static uint64_t began = 0;
static char status[40] = "Not made yet";

static void finish (void) {
    for (int i = 0; i < game_count; i++) {
        free(games[i].name);
    }
    free(games);
    games = NULL;
    game_count = 0;
    next = 0;
}

void art_report_cancel (void) {
    if (!games) {
        return;
    }
    if (out) {
        fclose(out);
        out = NULL;
        remove(REPORT_TMP_PATH);
    }
    finish();
    snprintf(status, sizeof(status), "Stopped");
    debugf("art report: stopped\n");
}

bool art_report_busy (void) {
    return games != NULL;
}

const char *art_report_status (void) {
    if (games) {
        snprintf(status, sizeof(status), "Checking %d of %d", next + 1, game_count);
    }
    return status;
}

/* Every cartridge game in the Games tab's folders, in their order. */
static void list_games (menu_t *menu) {
    const char *folders[GAMES_FOLDERS_MAX + 1];
    int count = games_folders_list(menu, folders);
    for (int f = 0; f < count; f++) {
        char folder[256];
        snprintf(folder, sizeof(folder), "%s%s", menu->storage_prefix, folders[f] + 1);
        dir_t info;
        int result = dir_findfirst(folder, &info);
        while (result == 0 && game_count < MAX_GAMES) {
            if (info.d_type != DT_DIR && file_has_extensions(info.d_name, rom_extensions)) {
                size_t length = strlen(folders[f]) + strlen(info.d_name) + 2;
                char *name = malloc(length);
                if (!name) {
                    return;
                }
                /* As the Games tab names them: the folder, then the file ("/" alone adds nothing). */
                snprintf(name, length, "%s%s%s", folders[f] + 1, folders[f][1] ? "/" : "", info.d_name);
                games[game_count].name = name;
                games[game_count].size = info.d_size;
                game_count++;
            }
            result = dir_findnext(folder, &info);
        }
    }
}

void art_report_start (menu_t *menu) {
    art_report_cancel();
    games = calloc(MAX_GAMES, sizeof(game_t));
    if (!games) {
        snprintf(status, sizeof(status), "Not enough memory");
        return;
    }
    began = get_ticks_ms();
    list_games(menu);
    out = fopen(REPORT_TMP_PATH, "w");
    if (!out || game_count == 0) {
        bool none = (game_count == 0);
        art_report_cancel();
        snprintf(status, sizeof(status), none ? "No games found" : "Could not write");
        return;
    }
    lacking = 0;
    fprintf(out, "missingart 1\n");
    fprintf(out, "# Games that lack art or details, made by SummerStation64. One game per line,\n");
    fprintf(out, "# tab-separated: art folder under menu/metadata, kind, what it lacks, the game's file.\n");
    fprintf(out, "# Lacks: front, back, top, cartridge, description, date, developer.\n");
    debugf("art report: %d games to check\n", game_count);
}

/* A file the card holds, from the menu's metadata folder. */
static bool on_card (const char *folder, const char *file) {
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", folder, file);
    return file_exists(path);
}

/* The ini has a short description ("short-desc = ..." with something after it). */
static bool has_short_desc (const char *ini) {
    FILE *f = fopen(ini, "r");
    if (!f) {
        return false;
    }
    char line[256];
    bool found = false;
    while (!found && fgets(line, sizeof(line), f)) {
        char *key = line;
        while (*key == ' ' || *key == '\t') key++;
        if (strncmp(key, "short-desc", 10) == 0) {
            char *equals = strchr(key, '=');
            if (equals) {
                for (char *c = equals + 1; *c && !found; c++) {
                    found = (*c != ' ' && *c != '\t' && *c != '\r' && *c != '\n');
                }
            }
        }
    }
    fclose(f);
    return found;
}

static void check (menu_t *menu, const game_t *g) {
    char path[300];
    snprintf(path, sizeof(path), "%s%s", menu->storage_prefix, g->name);

    char code[5] = "", title[GAME_INDEX_TITLE_LENGTH + 1] = "";
    const game_index_entry_t *known = game_index_find(path, g->size);
    if (known) {
        memcpy(code, known->code, 4);
    }
    bool homebrew = known && game_index_is_homebrew(code);
    if (!known || homebrew) {
        /* Not in the game list yet, or homebrew, whose art goes by its title. */
        if (!game_index_read_header(path, code, title)) {
            return;
        }
        code[4] = '\0';
        homebrew = game_index_is_homebrew(code);
    }

    /* Where its art is looked for on the card, as the covers look. */
    char folder[160], shown[64];
    if (homebrew) {
        snprintf(folder, sizeof(folder), "%smenu/metadata/homebrew/%s", menu->storage_prefix, title);
        int length = (int) strlen(title);
        while (length > 0 && title[length - 1] == ' ') {
            length--;
        }
        snprintf(shown, sizeof(shown), "homebrew/%.*s", length, title);
    } else {
        snprintf(folder, sizeof(folder), "%smenu/metadata/%c/%c/%c/%c", menu->storage_prefix, code[0], code[1], code[2], code[3]);
        snprintf(shown, sizeof(shown), "%c/%c/%c/%c", code[0], code[1], code[2], code[3]);
    }
    bool own_folder = directory_exists(folder);
    char art_folder[160];
    snprintf(art_folder, sizeof(art_folder), "%s", folder);
    if (!own_folder && !homebrew) {
        art_folder[strlen(art_folder) - 2] = '\0';  /* the folder without the region letter, if it is this game's */
        if (!directory_exists(art_folder) || !baked_art_card_ok(menu->storage_prefix, code)) {
            art_folder[0] = '\0';
        }
    } else if (!own_folder) {
        art_folder[0] = '\0';
    }

    const char *baked_code = homebrew ? NULL : code;
    bool front = (baked_code && baked_art_has(baked_code, IMAGE_BOXART_FRONT)) || (art_folder[0] && on_card(art_folder, "boxart_front.png"));
    bool back = (baked_code && baked_art_has(baked_code, IMAGE_BOXART_BACK)) || (art_folder[0] && on_card(art_folder, "boxart_back.png"));
    bool top = (baked_code && baked_art_has(baked_code, IMAGE_BOXART_TOP)) || (art_folder[0] && on_card(art_folder, "boxart_top.png"));
    bool cartridge = (baked_code && baked_art_has(baked_code, IMAGE_GAMEPAK_FRONT)) || (art_folder[0] && on_card(art_folder, "gamepak_front.png"));

    /* Details, where Game info reads them: "Game.metadata.ini" beside the
       file, else baked in, else the code's folder. Homebrew shares its code
       with unrelated games, so only the file beside it is its own. */
    char ini[320];
    snprintf(ini, sizeof(ini), "%s", path);
    char *dot = strrchr(ini, '.');
    if (dot && dot > strrchr(ini, '/')) {
        snprintf(dot, sizeof(ini) - (size_t) (dot - ini), ".metadata.ini");
    }
    bool have_ini = file_exists(ini);
    if (!have_ini && baked_code && baked_meta_path(baked_code, ini, sizeof(ini))) {
        have_ini = true;
    } else if (!have_ini && !homebrew) {
        snprintf(ini, sizeof(ini), "%s/metadata.ini", folder);
        have_ini = file_exists(ini);
    }
    uint16_t date = 0;
    char developer[GAME_INDEX_DEVELOPER_LENGTH + 1] = "";
    if (have_ini) {
        game_index_read_details(ini, &date, developer);
    }
    bool description = (have_ini && has_short_desc(ini)) || (baked_code && baked_meta_description(baked_code));

    char lacks[80] = "";
    #define LACKS(what) snprintf(lacks + strlen(lacks), sizeof(lacks) - strlen(lacks), "%s%s", lacks[0] ? "," : "", what)
    if (!front) LACKS("front");
    if (!back) LACKS("back");
    if (!top) LACKS("top");
    if (!cartridge) LACKS("cartridge");
    if (!description) LACKS("description");
    if (!date) LACKS("date");
    if (!developer[0]) LACKS("developer");
    #undef LACKS
    if (!lacks[0]) {
        return;
    }
    const char *kind = homebrew ? "homebrew" : (games_filter_is_hack_path(menu, path) || code[0] == 'X') ? "hack" : "game";
    fprintf(out, "%s\t%s\t%s\t%s\n", shown, kind, lacks, g->name);
    lacking++;
}

void art_report_step (menu_t *menu) {
    if (!games) {
        return;
    }
    if (next < game_count) {
        check(menu, &games[next]);
        next++;
        return;
    }
    bool ok = (fclose(out) == 0);
    out = NULL;
    if (!ok || !safe_file_replace(REPORT_TMP_PATH, REPORT_PATH)) {
        snprintf(status, sizeof(status), "Could not write");
        debugf("art report: could not write %s\n", REPORT_PATH);
    } else {
        snprintf(status, sizeof(status), "%d of %d games", lacking, game_count);
        debugf("art report: %d of %d games lack something, in %d ms\n", lacking, game_count, (int) (get_ticks_ms() - began));
    }
    finish();
}
