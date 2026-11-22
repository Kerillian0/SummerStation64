#include <fatfs/ff.h>
#include <libdragon.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "baked_art.h"
#include "baked_meta.h"
#include "fonts.h"
#include "game_index.h"
#include "games_ui.h"
#include "metadata_clear.h"
#include "path.h"
#include "sound.h"
#include "utils/fs.h"

typedef enum {
    CLEAR_ALL,
    CLEAR_PICTURES,
    CLEAR_DETAILS,
} clear_what_t;

static const char *const what_names[] = { "pictures and details", "pictures", "details" };

/* Every picture a game's folder may hold (box faces and cartridge). The
   title logo (logo.sprite) and preview clip (preview.h264, .seek) are not
   in the list, so they always stay. */
static const char *const pictures[] = {
    "boxart_front.png", "boxart_back.png", "boxart_top.png", "boxart_bottom.png",
    "boxart_left.png", "boxart_right.png", "gamepak_front.png", "gamepak_back.png",
};

#define MOST_FILES  (16)
#define PATH_MOST   (256)

typedef enum { PROMPT_NONE, PROMPT_ASK, PROMPT_TOLD } prompt_t;
static prompt_t prompt = PROMPT_NONE;
static bool back_to_covers = false;     /* after the answer: leave Game info so the change shows */
static clear_what_t what = CLEAR_ALL;
static char (*files)[PATH_MOST] = NULL; /* the files to move, while the question is up */
static int file_count = 0;
static bool built_in = false;           /* the menu has its own cover or details for this game */
static char rom[PATH_MOST];             /* the game's file, for forgetting its details */
static char text[320];

static void tell (const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    prompt = PROMPT_TOLD;
}

static void forget_files (void) {
    free(files);
    files = NULL;
    file_count = 0;
}

static void add_if_there (const char *path) {
    if (file_count < MOST_FILES && file_exists((char *) path)) {
        snprintf(files[file_count++], PATH_MOST, "%s", path);
    }
}

/* The game's own folder under menu/metadata, as the covers find it: the
   title's folder for homebrew; else the code's, or the one without the
   region letter when that one is this game's. Empty if it has none. */
static void art_folder (menu_t *menu, const char *code, bool homebrew, char *folder, size_t size) {
    if (homebrew) {
        char title[21];
        memcpy(title, menu->load.rom_info.title, 20);
        title[20] = '\0';
        snprintf(folder, size, "%smenu/metadata/homebrew/%s", menu->storage_prefix, title);
    } else {
        snprintf(folder, size, "%smenu/metadata/%c/%c/%c/%c", menu->storage_prefix, code[0], code[1], code[2], code[3]);
        if (!directory_exists(folder)) {
            folder[strlen(folder) - 2] = '\0';
            if (!baked_art_card_ok(menu->storage_prefix, code)) {
                folder[0] = '\0';
            }
        }
    }
    if (folder[0] && !directory_exists(folder)) {
        folder[0] = '\0';
    }
}

static void gather (menu_t *menu) {
    const char *code = menu->load.rom_info.game_code;
    bool homebrew = game_index_is_homebrew(code);
    char folder[160];
    art_folder(menu, code, homebrew, folder, sizeof(folder));
    char path[PATH_MOST + 64];

    if (what != CLEAR_DETAILS && folder[0]) {
        for (size_t i = 0; i < sizeof(pictures) / sizeof(pictures[0]); i++) {
            snprintf(path, sizeof(path), "%s/%s", folder, pictures[i]);
            add_if_there(path);
        }
    }
    if (what != CLEAR_PICTURES) {
        /* Beside the game: "Game.metadata.ini", and its own description file. */
        snprintf(rom, sizeof(rom), "%s", path_get(menu->load.rom_path));
        const char *slash = strrchr(rom, '/');
        const char *dot = strrchr(rom, '.');
        int stem = (dot && (!slash || dot > slash)) ? (int) (dot - rom) : (int) strlen(rom);
        snprintf(path, sizeof(path), "%.*s.metadata.ini", stem, rom);
        add_if_there(path);
        const char *own = menu->load.rom_info.meta.long_description_fn;
        if (own && own[0] && slash && !strstr(own, "..") && !strchr(own, ':') && own[0] != '/') {
            snprintf(path, sizeof(path), "%.*s/%s", (int) (slash - rom), rom, own);
        } else {
            snprintf(path, sizeof(path), "%.*s.description.txt", stem, rom);
        }
        add_if_there(path);
        /* The code's folder: not for homebrew, whose code other games share. */
        if (!homebrew && folder[0]) {
            snprintf(path, sizeof(path), "%s/metadata.ini", folder);
            add_if_there(path);
            snprintf(path, sizeof(path), "%s/description.txt", folder);
            add_if_there(path);
        }
    }
    char baked[40];
    built_in = !homebrew && ((what != CLEAR_DETAILS && baked_art_has(code, IMAGE_BOXART_FRONT))
                             || (what != CLEAR_PICTURES && baked_meta_path(code, baked, sizeof(baked))));
}

/* A row of the list was chosen. */
static void choose (menu_t *menu, void *arg) {
    what = (clear_what_t) (intptr_t) arg;
    forget_files();
    files = malloc(MOST_FILES * PATH_MOST);
    if (!files) {
        tell("Not enough memory to look.\n\nNothing was changed.");
        return;
    }
    gather(menu);
    if (file_count == 0) {
        forget_files();
        tell("This game has no %s of its own\non the SD card to clear.%s", what_names[what],
             built_in ? "\n\nThe ones you see are built into\nthe menu, and can't be cleared." : "");
        return;
    }
    prompt = PROMPT_ASK;
}

/* Every file moved to sd:/menu/trash/<date-time>/, under its own path. */
static void clear (menu_t *menu) {
    char stamp[24];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        snprintf(stamp, sizeof(stamp), "%lu", (unsigned long) (get_ticks_ms() / 1000));
    }
    int moved = 0;
    size_t prefix = strlen(menu->storage_prefix);
    for (int i = 0; i < file_count; i++) {
        const char *from = files[i];
        const char *inside = (strncmp(from, menu->storage_prefix, prefix) == 0) ? from + prefix : from;
        char to[PATH_MOST + 64];
        snprintf(to, sizeof(to), "%smenu/trash/%s/%s", menu->storage_prefix, stamp, inside);
        char *slash = strrchr(to, '/');
        *slash = '\0';
        bool failed = directory_create(to);     /* true when it FAILS (stock utils/fs.c) */
        *slash = '/';
        if (failed || f_rename(strip_fs_prefix((char *) from), strip_fs_prefix(to)) != FR_OK) {
            debugf("metadata clear: could not move %s to %s\n", from, to);
            continue;
        }
        debugf("metadata clear: %s moved to %s\n", from, to);
        moved++;
    }
    if (moved > 0 && what != CLEAR_PICTURES) {
        game_index_set_details(rom, 0, 0, NULL);    /* its date and developer are no longer known */
    }
    baked_meta_forget();        /* the descriptions it kept may be the ones just moved */
    back_to_covers = (moved > 0);
    if (moved == file_count) {
        tell("Moved %d file%s to\nmenu/trash/%s.\n\nFind Missing Metadata lists the game\nagain, for the Companion to fill in.%s",
             moved, (moved == 1) ? "" : "s", stamp,
             built_in ? "\n\nCovers and details built into the\nmenu still show for this game." : "");
    } else {
        tell("Moved %d of %d files to\nmenu/trash/%s.\n\nThe others could not be moved\nand are where they were.",
             moved, file_count, stamp);
    }
    forget_files();
}

static component_context_menu_t clear_menu = {
    .list = {
        { .text = "Pictures and details", .action = choose, .arg = (void *) CLEAR_ALL },
        { .text = "Pictures only", .action = choose, .arg = (void *) CLEAR_PICTURES },
        { .text = "Details only", .action = choose, .arg = (void *) CLEAR_DETAILS },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

component_context_menu_t *metadata_clear_menu (void) {
    return &clear_menu;
}

bool metadata_clear_prompt_process (menu_t *menu) {
    if (prompt == PROMPT_NONE) {
        return false;
    }
    if (prompt == PROMPT_ASK) {
        if (menu->actions.enter) {
            clear(menu);
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            forget_files();
            prompt = PROMPT_NONE;
            sound_play_effect(SFX_EXIT);
        }
    } else if (menu->actions.enter || menu->actions.back) {
        prompt = PROMPT_NONE;
        sound_play_effect(SFX_EXIT);
        if (back_to_covers) {
            back_to_covers = false;
            menu->next_mode = games_ui_origin();    /* the covers, which read their art afresh */
        }
    }
    return true;
}

void metadata_clear_prompt_draw (void) {
    if (prompt == PROMPT_ASK) {
        ui_components_messagebox_draw(
            "Clear this game's %s?\n\n"
            "%d file%s will be moved to menu/trash\n"
            "on the SD card, so you can take\n"
            "them back on a PC. The title logo\n"
            "and preview clip stay.\n\n"
            "^%02XA: Clear^00    ^%02XB: Keep^00",
            what_names[what], file_count, (file_count == 1) ? "" : "s", STL_RED, STL_GREEN);
    } else if (prompt == PROMPT_TOLD) {
        ui_components_messagebox_draw("%s\n\nA: OK", text);
    }
}
