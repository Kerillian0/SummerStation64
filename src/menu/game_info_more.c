/**
 * @file game_info_more.c
 * @brief Game info for 64DD disks and emulator games, laid out like an N64
 *        cartridge's.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "baked_art.h"
#include "baked_meta.h"
#include "display_name.h"
#include "fonts.h"
#include "game_info_more.h"
#include "game_info_ui.h"
#include "ini_parser.h"
#include "retro.h"
#include "sound.h"
#include "ui_components.h"
#include "utils/fs.h"

#define DESC_MAX    (2048)

static bool is_retro = false;
static char path[300];              /* the game's full path */
static char code[5];                /* a disk's ID, for its pictures */
static component_boxart_t *art = NULL;
static bool back = false;           /* the back of the box is showing */
static bool details = false;

static char author[64], date[32], name[96], genre[40];
static int players = 0;
static char *description = NULL;

/* What a metadata.ini says, and the full text from a description.txt. Either may be NULL. */
static void read_meta (const char *ini_file, const char *text_file) {
    author[0] = date[0] = name[0] = genre[0] = '\0';
    players = 0;
    free(description);
    description = NULL;
    const char *file = ini_file;
    if (file && file_exists((char *) file)) {
        ini_t *ini = ini_try_load(file);
        if (ini) {
            snprintf(author, sizeof(author), "%s", ini_get_string(ini, "meta", "author", ""));
            char *bar = strchr(author, '|');    /* "Developer | Publisher": the developer */
            if (bar) {
                *bar = '\0';
                for (int n = strlen(author); n > 0 && author[n - 1] == ' '; n--) author[n - 1] = '\0';
            }
            snprintf(date, sizeof(date), "%s", ini_get_string(ini, "meta", "release-date", ""));
            snprintf(name, sizeof(name), "%s", ini_get_string(ini, "meta", "name", ""));
            snprintf(genre, sizeof(genre), "%s", ini_get_string(ini, "meta", "genre", ""));
            players = ini_get_int(ini, "meta", "num-players", 0);
            const char *desc = ini_get_string(ini, "meta", "short-desc", "");
            if (desc[0]) {
                description = strdup(desc);
            }
            ini_free(ini);
        }
    }
    FILE *f = (text_file && file_exists((char *) text_file)) ? fopen(text_file, "r") : NULL;
    if (f) {
        char *text = malloc(DESC_MAX);
        if (text) {
            size_t n = fread(text, 1, DESC_MAX - 1, f);
            text[n] = '\0';
            while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r' || text[n - 1] == ' ')) text[--n] = '\0';
            if (n > 0) {
                free(description);
                description = text;
            } else {
                free(text);
            }
        }
        fclose(f);
    }
}

static void load_art (menu_t *menu) {
    baked_art_release(art);
    art = NULL;
    file_image_type_t which = back ? IMAGE_BOXART_BACK : IMAGE_BOXART_FRONT;
    art = is_retro ? retro_art_load(menu->storage_prefix, path, which)
                   : baked_art_open(menu->storage_prefix, code, "", which);
}

static void opened (menu_t *menu) {
    back = false;
    details = false;
    load_art(menu);
}

void game_info_more_open_disk (menu_t *menu, const char *disk_path, const char id[4]) {
    is_retro = false;
    snprintf(path, sizeof(path), "%s", disk_path);
    memcpy(code, id, 4);
    code[4] = '\0';
    /* The details: baked into the menu file, else the card's folder for the ID (with or without its region letter). */
    char baked[64];
    if (baked_meta_path(code, baked, sizeof(baked))) {
        read_meta(baked, NULL);
    } else {
        char folder[300], ini_file[340], text_file[340];
        snprintf(folder, sizeof(folder), "%s/menu/metadata/%c/%c/%c/%c", menu->storage_prefix, code[0], code[1], code[2], code[3]);
        if (!directory_exists(folder)) {
            folder[strlen(folder) - 2] = '\0';
        }
        snprintf(ini_file, sizeof(ini_file), "%s/metadata.ini", folder);
        snprintf(text_file, sizeof(text_file), "%s/description.txt", folder);
        read_meta(ini_file, text_file);
    }
    const char *baked_text = baked_meta_description(code);
    if (baked_text && (!description || strlen(baked_text) > strlen(description))) {
        free(description);
        description = strdup(baked_text);
    }
    opened(menu);
}

void game_info_more_open_retro (menu_t *menu, const char *game_path) {
    is_retro = true;
    snprintf(path, sizeof(path), "%s", game_path);
    code[0] = '\0';
    char folder[300], ini_file[340], text_file[340];
    const char *base = strrchr(game_path, '/');
    base = base ? base + 1 : game_path;
    char title[200];
    snprintf(title, sizeof(title), "%s", base);
    char *dot = strrchr(title, '.');
    if (dot) {
        *dot = '\0';
    }
    const char *systems[RETRO_SYSTEMS] = { NULL, "nes", "snes", "gb", "gbc" };
    const char *system = systems[retro_system(game_path)];
    if (system) {
        snprintf(folder, sizeof(folder), "%s/menu/metadata/%s/%s", menu->storage_prefix, system, title);
        snprintf(ini_file, sizeof(ini_file), "%s/metadata.ini", folder);
        snprintf(text_file, sizeof(text_file), "%s/description.txt", folder);
        read_meta(ini_file, text_file);
    } else {
        read_meta(NULL, NULL);
    }
    opened(menu);
}

void game_info_more_close (void) {
    baked_art_release(art);
    art = NULL;
    free(description);
    description = NULL;
    details = false;
}

void game_info_more_pictures (menu_t *menu) {
    back = !back;
    load_art(menu);
    if (!art && back) {
        back = false;       /* no back picture: the front again */
        load_art(menu);
        sound_play_effect(SFX_ERROR);
        return;
    }
    sound_play_effect(SFX_CURSOR);
}

void game_info_more_details (bool show) {
    details = show;
}

bool game_info_more_details_showing (void) {
    return details;
}

bool game_info_more_backdrop_ready (void) {
    return game_info_ui_backdrop_ready_for(path);
}

/* "Platform · Oct 26, 2000 · Nintendo", or NULL: the developer last, so a long name is what gets cut short. */
static const char *above_line (void) {
    static char line[160];
    char released[32] = "";
    if (date[0]) {
        game_info_ui_format_date(released, sizeof(released), date);
    }
    const char *parts[3] = { genre, released, author };
    line[0] = '\0';
    for (int i = 0; i < 3; i++) {
        if (parts[i][0]) {
            size_t n = strlen(line);
            snprintf(line + n, sizeof(line) - n, "%s%s", n ? " \xC2\xB7 " : "", parts[i]);
        }
    }
    return line[0] ? line : NULL;
}

/* "1-4 players", or NULL for one player or not known. */
static const char *players_badge (void) {
    static char text[24];
    if (players < 2) {
        return NULL;
    }
    snprintf(text, sizeof(text), "1-%d players", players);
    return text;
}

static const char *file_name (void) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

void game_info_more_draw_disk (menu_t *menu, const disk_info_t *disk, int swap_disks, const char *cart, const char *needs) {
    static const char *regions[] = { "Development", "Japan", "USA" };
    const char *region = (disk->region >= 0 && disk->region <= DISK_REGION_USA) ? regions[disk->region] : "Unknown";
    char with[96] = "";
    game_info_page_t page = {
        .path = path,
        .name = name[0] ? name : display_name_info(file_name()),
        .above = above_line(),
        .fact_caption = "Region",
        .fact_value = region,
        .description = description,
        .a_hint = cart ? "Play with cartridge" : "Play",
        .z_hint = cart ? "Disk only" : NULL,
        .r_hint = "Options",
        .start_hint = "Details",
        .pictures = true,
        .front_picture = !back,
    };
    int b = 0;
    page.badges[b] = "64DD disk";
    page.badge_styles[b++] = STL_DEFAULT;
    if (cart) {
        snprintf(with, sizeof(with), "With %s", display_name_file(cart));
        page.badges[b] = with;
        page.badge_styles[b++] = STL_BLUE;
    } else if (needs) {
        page.badges[b] = "Needs its cartridge";
        page.badge_styles[b++] = STL_ORANGE;
    }
    if (players_badge()) {
        page.badges[b] = players_badge();
        page.badge_styles[b++] = STL_DEFAULT;
    }
    game_info_ui_draw_page(menu, art, &page);

    if (details) {
        ui_components_messagebox_draw(
            "Disk details\n\n"
            "Region: %s\nUnique ID: %.4s\nVersion: %d\nDisk type: %d\nSwap disks: %d found\n%s%s\n\n"
            "File: %s",
            region, disk->id, (int) disk->version, (int) disk->disk_type, swap_disks,
            cart ? "Game PAK: " : "", cart ? cart : "", file_name());
    }
}

void game_info_more_draw_retro (menu_t *menu, const char *system_name) {
    game_info_page_t page = {
        .path = path,
        .name = name[0] ? name : display_name_info(file_name()),
        .above = above_line(),
        .fact_caption = "System",
        .fact_value = retro_name(retro_system(path)),
        .description = description,
        .start_hint = "Details",
        .pictures = true,
        .front_picture = !back,
    };
    page.badges[0] = players_badge();
    page.badge_styles[0] = STL_DEFAULT;
    game_info_ui_draw_page(menu, art, &page);

    if (details) {
        ui_components_messagebox_draw("Game details\n\nEmulated system: %s\nFile: %s", system_name, file_name());
    }
}
