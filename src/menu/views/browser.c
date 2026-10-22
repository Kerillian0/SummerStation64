#include <errno.h>
#include <math.h>
#include <miniz.h>
#include <miniz_zip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../debug_stats.h"
#include "../cart_load.h"
#include "../fonts.h"
#include "../zip_entry_count.h"
#include "utils/fs.h"
#include "views.h"
#include "../sound.h"
#include "../theme.h"
#include "../controls.h"
#include "../play_stats.h"
#include "../carousel_art.h"
#include "../safe_mode.h"
#include "../folder_memory.h"
#include "../art_tint.h"
#include "../games_folders.h"
#include "../games_filter.h"
#include "../game_learn.h"
#include "../sort_order.h"
#include "../menu_options.h"
#include "../display_name.h"
#include "../games_ui.h"
#include "../box_motion.h"
#include "../cover_list.h"
#include "../carousel.h"
#include "../tabs.h"
#include "../folders_ui.h"
#include "../intro.h"
#include "../ui_components/constants.h"

static const char *archive_extensions[] = { "zip", NULL };
static const char *cheat_extensions[] = {"cht", "cheats", "datel", "gameshark", NULL};
static const char *disk_extensions[] = { "ndd", NULL };
static const char *emulator_extensions[] = { "nes", "sfc", "smc", "gb", "gbc", "sms", "gg", "sg", "chf", "rzx", "mx1", "mx2", "emu", NULL };
static const char *image_extensions[] = { "png", NULL };
static const char *music_extensions[] = { "mp3", NULL };
static const char *n64_rom_extensions[] = { "z64", "n64", "v64", "rom", NULL };
static const char *patch_extensions[] = { "bps", "ips", "aps", "ups", "xdelta", NULL };
static const char *save_extensions[] = { "sav", "eep", "sra", "srm", "fla", NULL };
static const char *text_extensions[] = { "txt", "ini", "yml", "yaml", NULL };
static const char *rom_meta_extensions[] = { "meta", "metadata", NULL };

#define ARCHIVE_MAX_ENTRIES_JUMPER_PAK 512
// Fixed cap keeps memory use predictable on 4MB systems when scanning huge folders.
#define DIRECTORY_MAX_ENTRIES_JUMPER_PAK 1024

// The browser screen is two tabs: Games (the games of the start folder, as
// covers) and Folders (everything, as a plain list). Each has its own place.
static bool folders_tab = false;
static path_t *folders_directory = NULL;    // where the Folders tab was left
static char left_on[2][256] = { "", "" };   // the entry the Games tab [0] and the Folders tab [1] were left on, to go back to it (while the console is on)

static bool archive_entry_limit_exceeded = false;
static bool archive_entry_precheck_failed = false;
static bool directory_entry_limit_exceeded = false;

static const char *hidden_root_paths[] = {
    "/menu.bin",
    "/menu",
    "/N64FlashcartMenu.n64",
    "/ED64",
    "/ED64P",
    "/sc64menu.n64",
    // Windows garbage
    "/System Volume Information",
    // macOS garbage
    "/.fseventsd",
    "/.Spotlight-V100",
    "/.Trashes",
    "/.VolumeIcon.icns",
    "/.metadata_never_index",
    NULL,
};

struct substr { const char *str; size_t len; };
#define substr(str) ((struct substr){ str, sizeof(str) - 1 })

static const struct substr hidden_basenames[] = {
    substr("desktop.ini"), // Windows Explorer settings
    substr("Thumbs.db"),   // Windows Explorer thumbnails
    substr(".DS_Store"),   // macOS Finder settings
};
#define HIDDEN_BASENAMES_COUNT (sizeof(hidden_basenames) / sizeof(hidden_basenames[0]))

static const struct substr hidden_prefixes[] = {
    substr("._"), // macOS "AppleDouble" metadata files
};
#define HIDDEN_PREFIXES_COUNT (sizeof(hidden_prefixes) / sizeof(hidden_prefixes[0]))

// static bool file_is_fat_hidden (const char *full_path) {
//     struct stat st;
    
//     if (stat(full_path, &st) == 0) {
//         return FAT_ATTR_IS_HID(&st);
//     }
    
//     return false;
// }

static bool path_is_hidden (path_t *path) {
    char *stripped_path = strip_fs_prefix(path_get(path));

    // Check for hidden files based on full path
    for (size_t i = 0; hidden_root_paths[i] != NULL; i++) {
        if (strcmp(stripped_path, hidden_root_paths[i]) == 0) {
            return true;
        }
    }

    char *basename = file_basename(stripped_path);
    size_t basename_len = strlen(basename);

    // Check for hidden files based on filename
    for (size_t i = 0; i < HIDDEN_BASENAMES_COUNT; i++) {
        if (basename_len == hidden_basenames[i].len &&
            strncmp(basename, hidden_basenames[i].str, hidden_basenames[i].len) == 0) {
            return true;
        }
    }
    
    // Check for hidden files based on filename prefix
    for (size_t i = 0; i < HIDDEN_PREFIXES_COUNT; i++) {
        if (basename_len > hidden_prefixes[i].len &&
            strncmp(basename, hidden_prefixes[i].str, hidden_prefixes[i].len) == 0) {
            return true;
        }
    }

    // if (file_is_fat_hidden(path_get(path))) {
    //     return true;
    // }

    return false;
}

static int compare_entry (const void *pa, const void *pb) {
    entry_t *a = (entry_t *) (pa);
    entry_t *b = (entry_t *) (pb);

    if (a->type != b->type) {
        if (a->type == ENTRY_TYPE_DIR) {
            return -1;
        } else if (b->type == ENTRY_TYPE_DIR) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ARCHIVE) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ARCHIVE) {
            return 1;
        } else if (a->type == ENTRY_TYPE_DISK) {
            return -1;
        } else if (b->type == ENTRY_TYPE_DISK) {
            return 1;
        } else if (a->type == ENTRY_TYPE_EMULATOR) {
            return -1;
        } else if (b->type == ENTRY_TYPE_EMULATOR) {
            return 1;
        } else if (a->type == ENTRY_TYPE_IMAGE) {
            return -1;
        } else if (b->type == ENTRY_TYPE_IMAGE) {
            return 1;
        } else if (a->type == ENTRY_TYPE_MUSIC) {
            return -1;
        } else if (b->type == ENTRY_TYPE_MUSIC) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM_CHEAT) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM_CHEAT) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM_PATCH) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM_PATCH) {
            return 1;
        } else if (a->type == ENTRY_TYPE_SAVE) {
            return -1;
        } else if (b->type == ENTRY_TYPE_SAVE) {
            return 1;
        } else if (a->type == ENTRY_TYPE_TEXT) {
            return -1;
        } else if (b->type == ENTRY_TYPE_TEXT) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM_META) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM_META) {
            return 1;
        }
    }

    return strcasecmp(entry_file_name(a->name), entry_file_name(b->name)); // by the file's name, not the Games tab's folder in front of it
}

static void browser_list_free (menu_t *menu) {
    if (menu->browser.archive) {
        mz_zip_reader_end(&menu->browser.zip);
    }
    menu->browser.archive = false;

    for (int i = games_filter_all_entries(menu->browser.entries) - 1; i >= 0; i--) { // the Games filter's hidden games too
        free(menu->browser.list[i].name);
    }
    games_filter_forget();

    free(menu->browser.list);

    menu->browser.list = NULL;
    menu->browser.list_capacity = 0;
    menu->browser.entries = 0;
    menu->browser.entry = NULL;
    menu->browser.selected = -1;
}

static bool browser_list_reserve(menu_t *menu, int32_t required) {
    if (required <= menu->browser.list_capacity) {
        return false;
    }

    int32_t new_capacity = menu->browser.list_capacity > 0 ? menu->browser.list_capacity : 32;
    while (new_capacity < required) {
        int32_t growth = new_capacity / 2;
        if (growth <= 0 || new_capacity > INT32_MAX - growth) {
            return true;
        }
        new_capacity += growth;
    }

    entry_t *grown = realloc(menu->browser.list, new_capacity * sizeof(entry_t));
    if (!grown) {
        return true;
    }

    menu->browser.list = grown;
    menu->browser.list_capacity = new_capacity;
    return false;
}

static bool load_archive (menu_t *menu) {
    browser_list_free(menu);
    archive_entry_limit_exceeded = false;
    archive_entry_precheck_failed = false;

    uint64_t prechecked_entries = 0;
    if (!zip_try_read_entry_count(path_get(menu->browser.directory), &prechecked_entries)) {
        archive_entry_precheck_failed = true;
        return true;
    }

    if (!is_memory_expanded() && prechecked_entries > ARCHIVE_MAX_ENTRIES_JUMPER_PAK) {
        archive_entry_limit_exceeded = true;
        return true;
    }

    mz_zip_zero_struct(&menu->browser.zip);
    if (!mz_zip_reader_init_file(&menu->browser.zip, path_get(menu->browser.directory), 0)) {
        return true;
    }

    menu->browser.archive = true;
    int32_t zip_entries = (int32_t)mz_zip_reader_get_num_files(&menu->browser.zip);

    if (!is_memory_expanded() && zip_entries > ARCHIVE_MAX_ENTRIES_JUMPER_PAK) {
        archive_entry_limit_exceeded = true;
        browser_list_free(menu);
        return true;
    }

    menu->browser.entries = 0;

    for (int32_t i = 0; i < zip_entries; i++) {
        if (browser_list_reserve(menu, menu->browser.entries + 1)) {
            browser_list_free(menu);
            return true;
        }

        entry_t *entry = &menu->browser.list[menu->browser.entries];

        mz_zip_archive_file_stat info;
        if (!mz_zip_reader_file_stat(&menu->browser.zip, i, &info)) {
            browser_list_free(menu);
            return true;
        }

        entry->name = strdup(info.m_filename);
        if (!entry->name) {
            browser_list_free(menu);
            return true;
        }

        entry->type = ENTRY_TYPE_ARCHIVED;
        entry->size = info.m_uncomp_size;
        entry->index = i;
        menu->browser.entries++;
    }

    if (menu->browser.entries > 0) {
        menu->browser.selected = 0;
        menu->browser.entry = &menu->browser.list[menu->browser.selected];
    }

    qsort(menu->browser.list, menu->browser.entries, sizeof(entry_t), compare_entry);

    return false;
}

// "N64(JP)" + "Game.z64" -> "N64(JP)/Game.z64". The Games tab lists games from
// several folders, so each name carries its folder (see games_folders.h).
static char *name_with_prefix (const char *prefix, const char *name) {
    if (!prefix || !prefix[0]) {
        return strdup(name);
    }
    char *joined = malloc(strlen(prefix) + strlen(name) + 2);
    if (joined) {
        sprintf(joined, "%s/%s", prefix, name);
    }
    return joined;
}

// Add the entries of one folder to the list. Returns dir_findnext's last
// result (below -1 is a read error), or -2 if the list could not grow.
static int scan_folder (menu_t *menu, path_t *path, const char *prefix) {
    int result;
    dir_t info;

    result = dir_findfirst(path_get(path), &info);

    while (result == 0) {
        bool hide = false;

        if (!menu->settings.show_protected_entries) {
            path_push(path, info.d_name);
            hide = path_is_hidden(path);
            path_pop(path);
        }

        if (!menu->settings.show_saves_folder) {
            path_push(path, info.d_name);
            // Skip the "saves" directory if it is hidden (this is case sensitive)
            if (strcmp(info.d_name, SAVE_DIRECTORY_NAME) == 0) {
                hide = true;
            }
            path_pop(path);
        }

        if (!menu->settings.show_save_files) {
            path_push(path, info.d_name);
            // Skip save files if they are hidden (this is case sensitive)
            if (file_has_extensions(info.d_name, save_extensions)) {
                hide = true;
            }
            path_pop(path);
        }

        if (!menu->settings.show_cheat_files) {
            path_push(path, info.d_name);
            // Skip cheat files if they are hidden (this is case sensitive)
            if (file_has_extensions(info.d_name, cheat_extensions)) {
                hide = true;
            }
            path_pop(path);
        }

        if (!hide) {
            if (!is_memory_expanded() && menu->browser.entries >= DIRECTORY_MAX_ENTRIES_JUMPER_PAK) {
                directory_entry_limit_exceeded = true;
                return -2;
            }

            if (browser_list_reserve(menu, menu->browser.entries + 1)) {
                return -2;
            }

            entry_t *entry = &menu->browser.list[menu->browser.entries];

            entry->name = name_with_prefix(prefix, info.d_name);
            if (!entry->name) {
                return -2;
            }

            if (info.d_type == DT_DIR) {
                entry->type = ENTRY_TYPE_DIR;
            } else if (file_has_extensions(entry->name, n64_rom_extensions)) {
                entry->type = ENTRY_TYPE_ROM;
            } else if (file_has_extensions(entry->name, disk_extensions)) {
                entry->type = ENTRY_TYPE_DISK;
            } else if (file_has_extensions(entry->name, patch_extensions)) {
                entry->type = ENTRY_TYPE_ROM_PATCH;
            } else if (file_has_extensions(entry->name, cheat_extensions)) {
                entry->type = ENTRY_TYPE_ROM_CHEAT;
            } else if (file_has_extensions(entry->name, emulator_extensions)) {
                entry->type = ENTRY_TYPE_EMULATOR;
            } else if (file_has_extensions(entry->name, save_extensions)) {
                entry->type = ENTRY_TYPE_SAVE;
            } else if (file_has_extensions(entry->name, image_extensions)) {
                entry->type = ENTRY_TYPE_IMAGE;
            } else if (file_has_extensions(entry->name, text_extensions)) {
                entry->type = ENTRY_TYPE_TEXT;
            } else if (file_has_extensions(entry->name, music_extensions)) {
                entry->type = ENTRY_TYPE_MUSIC;
            } else if (file_has_extensions(entry->name, archive_extensions)) {
                entry->type = ENTRY_TYPE_ARCHIVE;
            } else if (file_has_extensions(entry->name, rom_meta_extensions)) {
                entry->type = ENTRY_TYPE_ROM_META;
            } else {
                entry->type = ENTRY_TYPE_OTHER;
            }

            // The Games tab lists games only.
            if (!folders_tab && entry->type != ENTRY_TYPE_ROM && entry->type != ENTRY_TYPE_DISK && entry->type != ENTRY_TYPE_EMULATOR) {
                free(entry->name);
                entry->name = NULL;
                result = dir_findnext(path_get(path), &info);
                continue;
            }

            entry->size = info.d_size;
            entry->index = menu->browser.entries;
            menu->browser.entries++;
        }

        result = dir_findnext(path_get(path), &info);
    }

    return result;
}

static bool load_directory (menu_t *menu) {
    int result;

    browser_list_free(menu);
    directory_entry_limit_exceeded = false;

    if (folders_tab) {
        path_t *path = path_clone(menu->browser.directory);
        result = scan_folder(menu, path, "");
        path_free(path);
    } else {
        // The Games tab: the start folder and every folder added to it
        // (games_folders.c), all listed from the top of the card.
        const char *folders[GAMES_FOLDERS_MAX + 1];
        int count = games_folders_list(menu, folders);
        result = -1;
        for (int i = 0; i < count; i++) {
            path_t *path = path_init(menu->storage_prefix, (char *) folders[i]);
            int r = scan_folder(menu, path, folders[i] + 1); // without the leading '/'
            path_free(path);
            if (r == -2) {
                result = r;
                break;
            }
            // A folder that has gone missing is left out, not an error.
        }
    }

    if (result < -1) {
        browser_list_free(menu);
        return true;
    }

    if (!folders_tab) games_filter_after_load(menu, compare_entry); // Games: hide what the region filter leaves out

    if (menu->browser.entries > 0) {
        menu->browser.selected = 0;
        menu->browser.entry = &menu->browser.list[menu->browser.selected];
    }

    sort_order_apply(menu, compare_entry); // the player's sort order; compare_entry is the stock one

    return false;
}

static bool reload_directory (menu_t *menu) {
    int selected = menu->browser.selected;

    if (load_directory(menu)) {
        return true;
    }

    menu->browser.selected = selected;
    if (menu->browser.selected >= menu->browser.entries) {
        menu->browser.selected = menu->browser.entries - 1;
    }
    menu->browser.entry = menu->browser.selected >= 0 ? &menu->browser.list[menu->browser.selected] : NULL;

    return false;
}

static bool push_directory (menu_t *menu, char *directory, bool archive) {
    path_t *previous_directory = path_clone(menu->browser.directory);

    path_push(menu->browser.directory, directory);

    if (archive ? load_archive(menu) : load_directory(menu)) {
        path_free(menu->browser.directory);
        menu->browser.directory = previous_directory;
        return true;
    }

    path_free(previous_directory);

    return false;
}

static bool pop_directory (menu_t *menu) {
    path_t *previous_directory = path_clone(menu->browser.directory);

    path_pop(menu->browser.directory);

    if (load_directory(menu)) {
        path_free(menu->browser.directory);
        menu->browser.directory = previous_directory;
        return true;
    }

    for (uint16_t i = 0; i < menu->browser.entries; i++) {
        if (strcmp(menu->browser.list[i].name, path_last_get(previous_directory)) == 0) {
            menu->browser.selected = i;
            menu->browser.entry = &menu->browser.list[menu->browser.selected];
            break;
        }
    }

    path_free(previous_directory);

    return false;
}

static bool select_file (menu_t *menu, path_t *file) {
    path_t *previous_directory = path_clone(menu->browser.directory);

    path_free(menu->browser.directory);
    menu->browser.directory = path_clone(file);
    path_pop(menu->browser.directory);

    if (load_directory(menu)) {
        path_free(menu->browser.directory);
        menu->browser.directory = previous_directory;
        return true;
    }

    for (uint16_t i = 0; i < menu->browser.entries; i++) {
        if (strcmp(menu->browser.list[i].name, path_last_get(file)) == 0) {
            menu->browser.selected = i;
            menu->browser.entry = &menu->browser.list[menu->browser.selected];
            break;
        }
    }

    path_free(previous_directory);

    return false;
}

static void show_properties (menu_t *menu, void *arg) {
    menu->next_mode = menu->browser.entry->type == ENTRY_TYPE_ARCHIVED ? MENU_MODE_EXTRACT_FILE : MENU_MODE_FILE_INFO;
}

static void show_game_info (menu_t *menu, void *arg) {
    if (menu->browser.entry->type == ENTRY_TYPE_ROM) {
        menu->next_mode = MENU_MODE_LOAD_ROM;
    } else {
        show_properties(menu, arg);
    }
}

static void delete_entry (menu_t *menu, void *arg) {
    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);

    if (remove(path_get(path))) {
        menu->browser.valid = false;
        if (menu->browser.entry->type == ENTRY_TYPE_DIR) {
            menu_show_error(menu, "Couldn't delete directory\nDirectory might not be empty");
        } else {
            menu_show_error(menu, "Couldn't delete file");
        }
        path_free(path);
        return;
    }

    path_free(path);

    if (reload_directory(menu)) {
        menu->browser.valid = false;
        menu_show_error(menu, "Couldn't refresh directory contents after delete operation");
    }
}

static void extract_entry (menu_t *menu, void *arg) {
    menu->load_pending.extract_file = true;
    menu->next_mode = MENU_MODE_EXTRACT_FILE;
}

static void note_show (const char *text);

static void set_default_directory (menu_t *menu, void *arg) {
    if (!folders_tab) {
        note_show("Go to the folder in the Folders tab first.");
        return;
    }
    free(menu->settings.default_directory);
    menu->settings.default_directory = strdup(strip_fs_prefix(path_get(menu->browser.directory)));
    settings_save(&menu->settings);
}

// A short note over the screen after a folder is added or removed.
static char note_text[96] = "";
static uint64_t note_until_ms = 0;

static void note_show (const char *text) {
    snprintf(note_text, sizeof(note_text), "%s", text);
    note_until_ms = get_ticks_ms() + 2000;
}

static void note_draw (void) {
    if (note_text[0] && get_ticks_ms() < note_until_ms) {
        ui_components_messagebox_draw("%s", note_text);
    }
}

static void add_games_folder (menu_t *menu, void *arg) {
    if (!folders_tab) {
        note_show("Go to the folder in the Folders tab first.");
    } else if (games_folders_add(path_get(menu->browser.directory))) {
        note_show("Added. Its games now show in the Games tab.");
    } else if (games_folders_count() >= GAMES_FOLDERS_MAX) {
        note_show("The Games tab already shows the most folders it can.");
    } else {
        note_show("This folder is already in the Games tab.");
    }
}

static void remove_games_folder (menu_t *menu, void *arg) {
    if (!folders_tab) {
        note_show("Go to the folder in the Folders tab first.");
    } else if (games_folders_remove(path_get(menu->browser.directory))) {
        note_show("Removed from the Games tab.");
    } else {
        note_show("This folder was not added to the Games tab.\n(The start folder is always shown.)");
    }
}

// Folders tab, Options: mark the folder shown as holding hacks (games_folders.h).
static void mark_hacks_folder (menu_t *menu, void *arg) {
    const char *here = path_get(menu->browser.directory);
    if (!folders_tab) {
        note_show("Go to the folder in the Folders tab first.");
    } else if (games_folders_is_hacks(here)) {
        note_show("This folder is already marked as hacks.");
    } else if (!games_folders_has(here) && !path_is_root(menu->browser.directory)
               && strcasecmp(here + 3, menu->settings.default_directory ? menu->settings.default_directory : "") != 0
               && !games_folders_add(here)) {
        note_show("The Games tab already shows the most folders it can.");
    } else {
        games_folders_mark_hacks(here, true);
        note_show("Marked. Its games are hacks in the Games tab.");
    }
}

static void unmark_hacks_folder (menu_t *menu, void *arg) {
    if (!folders_tab) {
        note_show("Go to the folder in the Folders tab first.");
    } else if (games_folders_mark_hacks(path_get(menu->browser.directory), false)) {
        note_show("No longer marked as hacks.");
    } else {
        note_show("This folder was not marked as hacks.");
    }
}

// Game Versions: Grouped. A on a cover that stands for several files (an
// original, its hacks, copies) lists them; the one played last is picked
// already. Choosing one carries on as A would have (info, or starting it).
#define VERSIONS_MAX    (12)
static entry_t *version_entries[VERSIONS_MAX];
static char version_names[VERSIONS_MAX][64];
static int version_default = 0;
static bool version_launch = false;     // the A press asked to start the game straight away

static void pick_version (menu_t *menu, void *arg) {
    menu->browser.entry = (entry_t *) arg;
    menu->next_mode = MENU_MODE_LOAD_ROM;
    controls_request_launch(version_launch);
}

static int versions_default_row (menu_t *menu) {
    (void) menu;
    return version_default;
}

static component_context_menu_t versions_menu = {
    .row_selected = -1,     // hidden until versions_open() shows it (0 would draw an empty list)
    .get_default_selection = versions_default_row,
    .list = {
        {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

// Opens the list if this cover stands for several files. @return true if it did.
static bool versions_open (menu_t *menu, entry_t *cover) {
    if (folders_tab || cover->type != ENTRY_TYPE_ROM || games_filter_versions(menu, cover) < 2) {
        return false;
    }
    int n = games_filter_version_list(menu, cover, version_entries, VERSIONS_MAX);
    time_t latest = 0;
    version_default = 0;
    for (int i = 0; i < n; i++) {
        bool hack = games_filter_is_hack(version_entries[i]);
        snprintf(version_names[i], sizeof(version_names[i]), "%s%s", entry_file_name(version_entries[i]->name), hack ? "  (hack)" : "");
        versions_menu.list[i].text = version_names[i];
        versions_menu.list[i].action = pick_version;
        versions_menu.list[i].arg = version_entries[i];
        path_t *path = path_clone_push(menu->browser.directory, version_entries[i]->name);
        time_t last = 0;
        if (play_stats_get(path_get(path), &last) > 0 && last > latest) {
            latest = last;
            version_default = i;
        }
        path_free(path);
    }
    versions_menu.list[n].text = NULL;
    version_launch = controls_consume_launch_request();
    ui_components_context_menu_init(&versions_menu);
    ui_components_context_menu_show(&versions_menu);
    return true;
}

// Options > "Random game" on the Games tab: jump to any other game in the row
// (within the filter in use). The row is a few dozen to a few hundred games,
// so the console's own random numbers are plenty.
static void random_game (menu_t *menu, void *arg) {
    if (menu->browser.entries < 2) {
        return;
    }
    int pick = (int) (getentropy32() % (uint32_t) (menu->browser.entries - 1));
    if (pick >= menu->browser.selected) {
        pick++;     // never the game already selected
    }
    menu->browser.selected = pick;
    menu->browser.entry = &menu->browser.list[pick];
}

// Every row the Options menu can have. entry_menu_fill() copies the ones
// that suit the tab into entry_context_menu each time it is opened.
static component_context_menu_t entry_menu_rows = {
    .list = {
        { .text = "Random game", .action = random_game },
        { .text = "Game info", .action = show_game_info },
        { .text = "Show entry properties", .action = show_properties },
        { .text = "Delete selected entry", .action = delete_entry },
        { .text = "Set current directory as default", .action = set_default_directory },
        { .text = "Add this folder to Games", .action = add_games_folder },
        { .text = "Remove this folder from Games", .action = remove_games_folder },
        { .text = "Mark this folder as hacks", .action = mark_hacks_folder },
        { .text = "Unmark this folder as hacks", .action = unmark_hacks_folder },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static component_context_menu_t entry_context_menu = {
    .list = {
        { .text = "Random game", .action = random_game },
        { .text = "Game info", .action = show_game_info },
        { .text = "Show entry properties", .action = show_properties },
        { .text = "Delete selected entry", .action = delete_entry },
        { .text = "Set current directory as default", .action = set_default_directory },
        { .text = "Add this folder to Games", .action = add_games_folder },
        { .text = "Remove this folder from Games", .action = remove_games_folder },
        { .text = "Mark this folder as hacks", .action = mark_hacks_folder },
        { .text = "Unmark this folder as hacks", .action = unmark_hacks_folder },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static void entry_menu_fill (void) {
    int rows = 0;
    for (int i = 0; entry_menu_rows.list[i].text != NULL; i++) {
        if (folders_tab && entry_menu_rows.list[i].action == random_game) {
            continue;   // a random pick is for the row of games
        }
        entry_context_menu.list[rows++] = entry_menu_rows.list[i];
    }
    entry_context_menu.list[rows].text = NULL;
    ui_components_context_menu_init(&entry_context_menu);   // counts the rows again
}

static component_context_menu_t archive_context_menu = {
    .list = {
        { .text = "Show entry properties", .action = show_properties },
        { .text = "Extract selected entry", .action = extract_entry },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static void set_menu_next_mode (menu_t *menu, void *arg) {
    menu_mode_t next_mode = (menu_mode_t) (arg);
    menu->next_mode = next_mode;
}

static component_context_menu_t settings_context_menu = {
    .list = {
        { .text = "Controller Pak manager", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_CONTROLLER_PAKFS) },
        { .text = "Menu settings", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_SETTINGS_EDITOR) },
        { .text = "Time (RTC) settings", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_RTC) },
        { .text = "Menu information", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_CREDITS) },
        { .text = "Flashcart information", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_FLASHCART) },
        { .text = "N64 information", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_SYSTEM_INFO) },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

// Switch the browser between its two tabs. The list is loaded again by
// view_browser_init(), which sees that it is no longer valid.
static void browser_show_tab (menu_t *menu, bool folders) {
    if (folders == folders_tab) {
        return;
    }

    carousel_art_reset();
    game_facts_reset();
    folder_memory_flush();
    folders_ui_forget(); // the Folders list's kept text

    if (folders_tab) {
        // Leaving Folders: remember where it was (a zip counts as its folder).
        if (menu->browser.archive) {
            path_pop(menu->browser.directory);
            menu->browser.archive = false;
        }
        if (folders_directory) {
            path_free(folders_directory);
        }
        folders_directory = path_clone(menu->browser.directory);
    }
    // Remember the entry this tab was on.
    snprintf(left_on[folders_tab ? 1 : 0], sizeof(left_on[0]), "%s", (menu->browser.entry && !menu->browser.archive) ? menu->browser.entry->name : "");

    folders_tab = folders;

    path_free(menu->browser.directory);
    if (folders && folders_directory) {
        menu->browser.directory = path_clone(folders_directory);
    } else if (folders) {
        menu->browser.directory = path_init(menu->storage_prefix, menu->settings.default_directory);
    } else {
        menu->browser.directory = path_init(menu->storage_prefix, "/"); // the Games tab lists its folders from the top of the card
    }
    if (!directory_exists(path_get(menu->browser.directory))) {
        path_free(menu->browser.directory);
        menu->browser.directory = path_init(menu->storage_prefix, "/");
    }

    menu->browser.valid = false;
}

// L or R: the tab before or after this one, in the player's order.
static void browser_step_tab (menu_t *menu, int direction) {
    games_tab_t here = folders_tab ? GAMES_TAB_FOLDERS : GAMES_TAB_GAMES;
    games_tab_t next = tabs_step(here, direction);
    if (next == here) {
        return;
    }
    sound_play_effect(SFX_CURSOR);
    tabs_open(menu, next);
    tabs_slide_begin(menu, direction); // the new tab's covers slide in from that side
    if (menu->next_mode == MENU_MODE_BROWSER) {
        view_browser_init(menu); // Games <-> Folders: the same screen with the other list
    }
}

static void process (menu_t *menu) {
    if (ui_components_context_menu_process(menu, menu->browser.archive ? &archive_context_menu : &entry_context_menu)) {
        return;
    }
    if (ui_components_context_menu_process(menu, &versions_menu)) {
        return;     // choosing which version of a game
    }

    if (settings_context_menu.row_selected >= 0 && menu->actions.settings) menu->actions.back = true; // START again closes the START menu, like B
    if (ui_components_context_menu_process(menu, &settings_context_menu)) {
        return;
    }

    controls_remap_tabs(menu, !folders_tab); // Games: left/right scroll the covers. Folders: up/down move down the list. L/R tabs, Z options
    if (!folders_tab && controls_consume_flip_request() && carousel_art_flip()) sound_play_effect(SFX_CURSOR); // up/down: turn the box over
    int filter_step = controls_consume_filter_step();
    if (!folders_tab && filter_step && games_filter_step(menu, filter_step)) sound_play_effect(SFX_CURSOR); // C-up/C-down: All, USA, Japan, Europe, 64DD

    int scroll_speed = menu->actions.go_fast ? 10 : 1;

    if (menu->browser.entries > 1) {
        if (menu->actions.go_up) {
            menu->browser.selected -= scroll_speed;
            if (menu->settings.wrap_file_list_scrolling) {
                // Wrap around to end if we go past the beginning
                menu->browser.selected = (menu->browser.selected % menu->browser.entries + menu->browser.entries) % menu->browser.entries;
            } else {
                // Clamp to beginning
                if (menu->browser.selected < 0) {
                    menu->browser.selected = 0;
                }
            }
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.go_down) {
            menu->browser.selected += scroll_speed;
            if (menu->settings.wrap_file_list_scrolling) {
                // Wrap around to beginning if we go past the end
                menu->browser.selected = menu->browser.selected % menu->browser.entries;
            } else {
                // Clamp to end
                if (menu->browser.selected >= menu->browser.entries) {
                    menu->browser.selected = menu->browser.entries - 1;
                }
            }
            sound_play_effect(SFX_CURSOR);
        }
        menu->browser.entry = &menu->browser.list[menu->browser.selected];
    }

    if (menu->actions.enter && menu->browser.entry) {
        sound_play_effect(SFX_ENTER);
        switch (menu->browser.entry->type) {
            case ENTRY_TYPE_ARCHIVE:
                if (push_directory(menu, menu->browser.entry->name, true)) {
                    menu->browser.valid = false;
                    menu_show_error(
                        menu,
                        archive_entry_limit_exceeded
                            ? "Archive is too large for Jumper Pak\nUse a smaller archive or an Expansion Pak"
                            : archive_entry_precheck_failed
                                ? "Could not inspect archive safely\nTry another archive"
                            : "Couldn't open file archive"
                    );
                }
                break;
            case ENTRY_TYPE_ARCHIVED:
                menu->next_mode = MENU_MODE_EXTRACT_FILE;
                break;
            case ENTRY_TYPE_DIR:
                if (push_directory(menu, menu->browser.entry->name, false)) {
                    menu->browser.valid = false;
                    menu_show_error(
                        menu,
                        directory_entry_limit_exceeded
                            ? "Directory is too large for Jumper Pak\nUse an Expansion Pak"
                            : "Couldn't open next directory"
                    );
                }
                break;
            case ENTRY_TYPE_DISK:
                menu->next_mode = MENU_MODE_LOAD_DISK;
                break;
            case ENTRY_TYPE_EMULATOR:
                menu->next_mode = MENU_MODE_LOAD_EMULATOR;
                break;
            case ENTRY_TYPE_IMAGE:
                menu->next_mode = MENU_MODE_IMAGE_VIEWER;
                break;
            case ENTRY_TYPE_MUSIC:
                menu->next_mode = MENU_MODE_MUSIC_PLAYER;
                break;
            case ENTRY_TYPE_ROM:
                if (!versions_open(menu, menu->browser.entry)) {
                    menu->next_mode = MENU_MODE_LOAD_ROM;
                }
                break;
            case ENTRY_TYPE_ROM_CHEAT:
                menu->next_mode = MENU_MODE_FILE_INFO; // FIXME: Implement MENU_MODE_LOAD_ROM_CHEAT.
                break;
            case ENTRY_TYPE_ROM_PATCH:
                menu->next_mode = MENU_MODE_FILE_INFO; // FIXME: Implement MENU_MODE_LOAD_ROM_PATCH.
                break;
            case ENTRY_TYPE_TEXT:
                menu->next_mode = MENU_MODE_TEXT_VIEWER;
                break;
            case ENTRY_TYPE_ROM_META:
                menu->next_mode = MENU_MODE_FILE_INFO; // FIXME: Implement MENU_MODE_LOAD_ROM_META.
                break;

            default:
                menu->next_mode = MENU_MODE_FILE_INFO;
                break;
        }
    } else if (menu->actions.back && folders_tab && !path_is_root(menu->browser.directory)) {
        if (pop_directory(menu)) {
            menu->browser.valid = false;
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Couldn't open last directory"
            );
        }
        sound_play_effect(SFX_EXIT);
    } else if (menu->actions.options && menu->browser.entry) {
        entry_menu_fill(); // "Random game" on the Games tab only
        ui_components_context_menu_show(menu->browser.archive ? &archive_context_menu : &entry_context_menu);
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.settings) {
        ui_components_context_menu_show(&settings_context_menu);
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.go_right) {
        browser_step_tab(menu, 1);
    } else if (menu->actions.go_left) {
        browser_step_tab(menu, -1);
    }
}

// --- Carousel prototype (step 1: placeholder boxes, no box art yet) ---
// Set to 0 to fall back to the stock file list.
#define BROWSER_CAROUSEL 1

#if BROWSER_CAROUSEL
#define CAROUSEL_CENTER_Y   180
#define CAROUSEL_CENTER_W   224
#define CAROUSEL_CENTER_H   160
#define CAROUSEL_SIDE_W     144
#define CAROUSEL_SIDE_H     104
#define CAROUSEL_GAP        16
#define CAROUSEL_ANGLED_W   (0.80f) // Angled Side Covers: a side cover's width when turned away...
#define CAROUSEL_ANGLED_FAR (0.90f) // ...and the height of its far edge, against the near one
#define CAROUSEL_SIDE_BOX_YAW (40.0f) // 3D Side Covers with Angled Side Covers on: degrees each box turns toward the middle

static const char *carousel_type_label (entry_type_t type) {
    switch (type) {
        case ENTRY_TYPE_DIR: return "FOLDER";
        case ENTRY_TYPE_ROM: return "N64";
        case ENTRY_TYPE_DISK: return "64DD";
        case ENTRY_TYPE_EMULATOR: return "EMU";
        case ENTRY_TYPE_ARCHIVE: return "ZIP";
        default: return "FILE";
    }
}

// Plain-language kind of entry, for the line under the title.
static const char *carousel_kind_label (entry_type_t type) {
    switch (type) {
        case ENTRY_TYPE_DIR: return "Folder";
        case ENTRY_TYPE_ROM: return "Nintendo 64 game";
        case ENTRY_TYPE_DISK: return "64DD disk";
        case ENTRY_TYPE_EMULATOR: return "Game for an emulator";
        case ENTRY_TYPE_ARCHIVE: return "Zip archive";
        case ENTRY_TYPE_IMAGE: return "Picture";
        case ENTRY_TYPE_MUSIC: return "Music";
        case ENTRY_TYPE_TEXT: return "Text";
        default: return "File";
    }
}

// Hollow frame: four thin bars, so whatever is behind stays visible. With
// Selection Marker on Corners, only a bracket at each corner.
static void carousel_ring (int x0, int y0, int x1, int y1, int thickness, color_t color) {
    rdpq_set_mode_fill(color);
    if (options_get(OPTION_SELECTION) == SELECTION_CORNERS) {
        int arm = ((x1 - x0) < (y1 - y0) ? (x1 - x0) : (y1 - y0)) / 7;    // each bracket's arms, along its two sides
        thickness += 1;                                                     // a little bolder, as there is less of it
        rdpq_fill_rectangle(x0, y0, x0 + arm, y0 + thickness);              // top left
        rdpq_fill_rectangle(x0, y0 + thickness, x0 + thickness, y0 + arm);
        rdpq_fill_rectangle(x1 - arm, y0, x1, y0 + thickness);              // top right
        rdpq_fill_rectangle(x1 - thickness, y0 + thickness, x1, y0 + arm);
        rdpq_fill_rectangle(x0, y1 - thickness, x0 + arm, y1);              // bottom left
        rdpq_fill_rectangle(x0, y1 - arm, x0 + thickness, y1 - thickness);
        rdpq_fill_rectangle(x1 - arm, y1 - thickness, x1, y1);              // bottom right
        rdpq_fill_rectangle(x1 - thickness, y1 - arm, x1, y1 - thickness);
        return;
    }
    rdpq_fill_rectangle(x0, y0, x1, y0 + thickness);
    rdpq_fill_rectangle(x0, y1 - thickness, x1, y1);
    rdpq_fill_rectangle(x0, y0 + thickness, x0 + thickness, y1 - thickness);
    rdpq_fill_rectangle(x1 - thickness, y0 + thickness, x1, y1 - thickness);
}

static float carousel_lerp (float a, float b, float t) {
    return a + (b - a) * t;
}

// Where the covers and the ring were drawn this frame, for the fade-in, which
// darkens only the parts of the screen that have something on them.
typedef struct { int x0, y0, x1, y1; } carousel_box_t;
static carousel_box_t drawn_covers[8];
static int drawn_count = 0;
static carousel_box_t drawn_center;     // the ring and the cover inside it
static bool drawn_moving = false;       // covers are sliding: their places overlap the ring's

// The fade-in's darkness for the next carousel_draw() only (0 = none). Covers
// that have their art are then drawn darkened, which is free, and left out of
// the places handed to the fade; only covers still without art, and their
// text, are darkened the costly way.
static int carousel_dark = 0;

static color_t carousel_dimmed (color_t c, int bright) {
    return RGBA32(c.r * bright / 255, c.g * bright / 255, c.b * bright / 255, c.a);
}

static void carousel_box_add (carousel_box_t *box, int x0, int y0, int x1, int y1) {
    if (x0 < box->x0) box->x0 = x0;
    if (y0 < box->y0) box->y0 = y0;
    if (x1 > box->x1) box->x1 = x1;
    if (y1 > box->y1) box->y1 = y1;
}

// Hand the fade the parts the covers take up; none overlap.
static void carousel_fade_rects (void (*rect) (int, int, int, int)) {
    if (drawn_moving) {
        carousel_box_t all = drawn_center;
        for (int i = 0; i < drawn_count; i++) {
            carousel_box_add(&all, drawn_covers[i].x0, drawn_covers[i].y0, drawn_covers[i].x1, drawn_covers[i].y1);
        }
        rect(all.x0, all.y0, all.x1, all.y1);
        return;
    }
    rect(drawn_center.x0, drawn_center.y0, drawn_center.x1, drawn_center.y1);
    for (int i = 0; i < drawn_count; i++) {
        rect(drawn_covers[i].x0, drawn_covers[i].y0, drawn_covers[i].x1, drawn_covers[i].y1);
    }
}

#define RING_SHAPE_MS   (180)   // how long the ring takes to change shape

// Shrink a cover's box to the shape of its art, keeping its middle.
static void carousel_fit_to_art (menu_t *menu, int index, int *w, int *h) {
    float aspect;
    if (!carousel_art_aspect(menu, index, &aspect) || aspect <= 0.0f) {
        return;
    }
    if ((float) *w / *h > aspect) {
        *w = (int) (*h * aspect);
    } else {
        *h = (int) (*w / aspect);
    }
}

// A plain cover turned away (Angled Side Covers), for a game with no art yet.
static void carousel_angled_box (int x0, int y0, int w, int h, float left, float right, color_t color, int alpha) {
    float cy = y0 + h / 2.0f;
    float tl[2] = { x0, cy - h * left / 2.0f };
    float tr[2] = { x0 + w, cy - h * right / 2.0f };
    float br[2] = { x0 + w, cy + h * right / 2.0f };
    float bl[2] = { x0, cy + h * left / 2.0f };
    rdpq_mode_push();
    rdpq_set_mode_standard();
    rdpq_set_prim_color(RGBA32(color.r, color.g, color.b, alpha));
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    if (alpha < 255) {
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    }
    rdpq_triangle(&TRIFMT_FILL, tl, tr, br);
    rdpq_triangle(&TRIFMT_FILL, tl, br, bl);
    rdpq_mode_pop();
}

// The ring's size: the selected cover's shape, eased so it changes smoothly.
static void carousel_ring_size (menu_t *menu, int selected, int *w, int *h) {
    static float ring_w = -1.0f, ring_h;
    static uint64_t last_ms = 0;
    uint64_t now = get_ticks_ms();

    int target_w = CAROUSEL_CENTER_W, target_h = CAROUSEL_CENTER_H;
    carousel_fit_to_art(menu, selected, &target_w, &target_h);

    if (ring_w < 0.0f || (now - last_ms) > 500) {
        ring_w = target_w; ring_h = target_h;   // first frame on this screen: no easing
    } else {
        float step = (float) (now - last_ms) / RING_SHAPE_MS;
        if (step > 1.0f) step = 1.0f;
        ring_w = carousel_lerp(ring_w, target_w, step);
        ring_h = carousel_lerp(ring_h, target_h, step);
    }
    last_ms = now;
    *w = (int) (ring_w + 0.5f);
    *h = (int) (ring_h + 0.5f);
}

#define RING_FADE_MS    (250)   // how long the ring takes to change to a new cover's color

#define RING_WAIT_MS    (400)   // how long the ring waits for a cover's color before settling for the accent

// The ring's color: the selected cover's own color when Tint Ring From Art is
// Everywhere, else the accent color; it fades from one to the next instead
// of jumping. While a cover's art is still on its way the ring keeps the
// color it has (so it does not flash the accent color between two covers),
// and when the screen has only just appeared it is not drawn at all for a
// moment (`*hide`), so it does not show the accent first and then change.
static color_t carousel_ring_color (menu_t *menu, int selected, color_t accent, bool *hide) {
    static float r = -1.0f, g, b;
    static uint64_t last_ms = 0;
    static uint64_t appeared_ms = 0;
    static bool settled = false;    // the ring has had a real color since the screen appeared
    uint64_t now = get_ticks_ms();
    *hide = false;

    if (r < 0.0f || (now - last_ms) > 500) {
        appeared_ms = now;          // first frame on this screen
        settled = false;
    }

    color_t target = accent;
    art_tint_state_t state = art_tint_on_covers() ? carousel_art_tint_state(menu, selected, &target) : ART_TINT_NONE;
    if (state == ART_TINT_PENDING && (now - appeared_ms) < RING_WAIT_MS * (settled ? 4 : 1)) {
        last_ms = now;
        if (!settled) {
            *hide = true;           // nothing to show yet
            return accent;
        }
        return RGBA32((int) r, (int) g, (int) b, 0xFF);    // keep the color it has
    }
    if (state == ART_TINT_PENDING) {
        target = accent;            // waited long enough
    }

    if (!settled) {
        r = target.r; g = target.g; b = target.b;   // its first color: no fade
        settled = true;
    } else {
        float step = (float) (now - last_ms) / RING_FADE_MS;
        if (step > 1.0f) step = 1.0f;
        r = carousel_lerp(r, target.r, step);
        g = carousel_lerp(g, target.g, step);
        b = carousel_lerp(b, target.b, step);
    }
    if (state != ART_TINT_PENDING) {
        appeared_ms = now;          // the wait for the next cover's color starts from the last one known
    }
    last_ms = now;
    return RGBA32((int) r, (int) g, (int) b, 0xFF);
}

#define CAROUSEL_SLIDE_MS   160     // how long a slide takes
#define CAROUSEL_TAB_SLIDE_PX   200 // how far to the side a newly opened tab's covers start
#define CAROUSEL_RAPID_MS   120     // moves closer together than this snap instead of sliding

static int slide_selected = -1;
static entry_t *slide_list = NULL;
static int slide_entries = 0;
static int slide_dir = 0;
static uint64_t slide_start_ms = 0;
static uint64_t slide_last_move_ms = 0;

// How far the covers still have to travel, in cover positions: +1 or -1 at
// the start of a slide, 0 at rest. Timed by the clock, not by frames, so it
// takes the same time at 50 and 60 Hz.
static float carousel_slide (menu_t *menu) {
    const cover_list_t *covers = cover_list_current(menu);
    uint64_t now = get_ticks_ms();
    int selected = covers->selected;
    bool same_folder = (covers->list == slide_list) && (covers->entries == slide_entries);

    if (!same_folder || slide_selected < 0) {
        slide_dir = 0;
    } else if (selected != slide_selected) {
        int delta = selected - slide_selected;
        bool rapid = (now - slide_last_move_ms) < CAROUSEL_RAPID_MS;
        if (features_enabled(FEATURE_CAROUSEL_ANIMATION) && !rapid) {
            // Wrapping from the last item to the first still counts as "next".
            if (menu->settings.wrap_file_list_scrolling && covers->entries > 2 && (delta == covers->entries - 1 || delta == 1 - covers->entries)) {
                delta = -delta;
            }
            slide_dir = (delta > 0) ? 1 : -1;
            slide_start_ms = now;
        } else {
            slide_dir = 0; // holding the direction or fast scroll: keep up, don't slide
        }
        slide_last_move_ms = now;
    }

    slide_list = covers->list;
    slide_entries = covers->entries;
    slide_selected = selected;

    if (slide_dir == 0) {
        return 0.0f;
    }

    float progress = (float) (now - slide_start_ms) / CAROUSEL_SLIDE_MS;
    if (progress >= 1.0f) {
        slide_dir = 0;
        return 0.0f;
    }

    float remaining = (1.0f - progress) * (1.0f - progress); // fast at first, easing into place
    return slide_dir * remaining;
}

// Not static: the Recent tab draws its covers with this too (see carousel.h).
void carousel_draw (menu_t *menu) {
    const theme_t *t = theme_get();
    const cover_list_t *covers = cover_list_current(menu);
    if (covers->entries <= 0 || covers->selected < 0 || covers->selected >= covers->entries) {
        return;
    }
    entry_t *selected_entry = &covers->list[covers->selected];

    const int screen_w = (int) display_get_width();
    const int cx = screen_w / 2;
    const bool side_covers = features_enabled(FEATURE_SIDE_COVERS);
    const bool see_through = features_enabled(FEATURE_SEE_THROUGH_COVERS);
    const bool angled = side_covers && features_enabled(FEATURE_ANGLED_COVERS);


    drawn_count = 0;
    drawn_center = (carousel_box_t) { 10000, 10000, -10000, -10000 };

    const float slide = carousel_slide(menu);
    const int tab_shift = (int) (tabs_slide() * CAROUSEL_TAB_SLIDE_PX); // just switched tab: the whole row comes in from the side
    drawn_moving = (slide != 0.0f) || (tab_shift != 0);
    const int bright = drawn_moving ? 255 : (255 - carousel_dark); // while covers slide, the fade darkens the whole row at once
    carousel_dark = 0;
    bool center_dimmed = false;
    const int side_w = angled ? (int) (CAROUSEL_SIDE_W * CAROUSEL_ANGLED_W) : CAROUSEL_SIDE_W; // turned covers take less room
    const int first = CAROUSEL_CENTER_W / 2 + CAROUSEL_GAP + side_w / 2;
    const int step = side_w + CAROUSEL_GAP;

    // Draw outer covers first so the center one sits on top. While sliding,
    // every cover sits a fraction of a position away from where it will rest.
    for (int dist = 3; dist >= 0; dist--) {
        for (int sign = -1; sign <= 1; sign += 2) {
            if (dist == 0 && sign == 1) {
                continue; // center is drawn once
            }
            int offset = dist * sign;
            int i = cover_list_neighbor(menu, offset); // with Wrap File List on, the last game sits left of the first
            if (i < 0) {
                continue;
            }

            float pos = offset + slide;         // 0 = center, +1/-1 = next to it, ...
            float away = fabsf(pos);
            bool resting_center = (offset == 0) && (slide == 0.0f);

            // Solid in the center, more see-through the further out, so the
            // themed background shows through. Without side covers a cover
            // simply fades out as it leaves the center.
            float alpha;
            if (side_covers && !see_through) {
                // Solid side covers; they still fade out as they leave the row.
                alpha = (away < 2.0f) ? 255.0f : carousel_lerp(255.0f, 0.0f, away - 2.0f);
            } else if (side_covers) {
                if (away < 1.0f) alpha = carousel_lerp(255.0f, 176.0f, away);
                else if (away < 2.0f) alpha = carousel_lerp(176.0f, 112.0f, away - 1.0f);
                else alpha = carousel_lerp(112.0f, 0.0f, away - 2.0f);
            } else {
                alpha = carousel_lerp(255.0f, 0.0f, away);
            }
            if (alpha <= 0.0f) {
                continue;
            }

            // Size shrinks from the center size to the side size over the first position.
            float grow = (away < 1.0f) ? (1.0f - away) : 0.0f;
            int w = (int) carousel_lerp(CAROUSEL_SIDE_W, CAROUSEL_CENTER_W, grow);
            int h = (int) carousel_lerp(CAROUSEL_SIDE_H, CAROUSEL_CENTER_H, grow);
            carousel_fit_to_art(menu, i, &w, &h); // a tall Japanese box is drawn tall, without bars at its sides

            // Angled Side Covers: a cover turns away as it leaves the center,
            // its far edge (toward the screen's side) shorter than its near one.
            float turn = angled ? ((away < 1.0f) ? away : 1.0f) : 0.0f;
            float far = carousel_lerp(1.0f, CAROUSEL_ANGLED_FAR, turn);
            const int box_w = w;    // 3D Side Covers: a box narrows itself as it turns
            w = (int) (w * carousel_lerp(1.0f, CAROUSEL_ANGLED_W, turn));

            // Horizontal center of this cover.
            float mid = (away < 1.0f) ? (pos * first) : ((pos < 0.0f ? -1.0f : 1.0f) * (first + (away - 1.0f) * step));
            int x0 = cx + tab_shift + (int) mid - w / 2;
            int y0 = CAROUSEL_CENTER_Y - h / 2;
            // Box Hover's float: the side covers bob too, each in its own time,
            // less and less toward the middle, where the 3D Box has its own.
            // In 1-pixel steps for art, 2 for a cover showing text (which
            // would shimmer on 240p).
            float art_shape;
            float lift = box_motion_side_lift(i, (away < 1.0f) ? away : 1.0f);
            y0 += carousel_art_aspect(menu, i, &art_shape) ? (int) lroundf(lift) : ((int) lroundf(lift / 2.0f)) * 2;
            if (x0 < 0 || (x0 + w) > screen_w) {
                continue; // still off the edge of the screen
            }
            // During the fade-in a cover with its art is drawn already
            // darkened; one without (text on a plain box) is handed to the
            // fade to darken afterwards.
            float has_art_shape;
            bool dim_here = (bright < 255) && carousel_art_aspect(menu, i, &has_art_shape)
                && !(resting_center && carousel_art_flip_width() < 1.0f);
            int cover_bright = dim_here ? bright : 255;
            color_t panel = carousel_dimmed(t->panel, cover_bright);
            if (dim_here) {
                if (offset == 0) center_dimmed = true;
            } else if (offset == 0) {
                carousel_box_add(&drawn_center, x0, y0, x0 + w, y0 + h);
            } else if (drawn_count < 8) {
                drawn_covers[drawn_count++] = (carousel_box_t) { x0, y0, x0 + w, y0 + h };
            }
            carousel_art_brightness(cover_bright);

            // 3D Side Covers: every cover but the selected one at rest is a box
            // too, turned to face the middle with Angled Side Covers on. A cover
            // sliding into the middle turns round to the selected box's angle.
            if (!resting_center) {
                float edge_turn = (away < 1.0f) ? away : 1.0f;
                float side_yaw = angled ? ((pos < 0.0f) ? -CAROUSEL_SIDE_BOX_YAW : CAROUSEL_SIDE_BOX_YAW) : CAROUSEL_BOX_YAW;
                float yaw = carousel_lerp(CAROUSEL_BOX_YAW, side_yaw, edge_turn);
                int bx0 = x0 + w / 2 - box_w / 2;
                if (carousel_art_draw_side_box(menu, i, bx0, y0, box_w, h, yaw, (int) alpha)) {
                    carousel_art_brightness(255);
                    continue;
                }
            }

            if (turn > 0.0f) {
                float left = (pos < 0.0f) ? far : 1.0f;
                float right = (pos < 0.0f) ? 1.0f : far;
                if (!carousel_art_draw_angled(menu, i, x0, y0, w, h, left, right, (int) alpha)) {
                    carousel_angled_box(x0, y0, w, h, left, right, panel, (int) alpha); // no art yet: a plain box, no text
                }
                carousel_art_brightness(255);
                continue;
            }

            if (resting_center && features_enabled(FEATURE_BOX_3D) && carousel_art_draw_box(menu, i, x0, y0, w, h)) {
                carousel_art_brightness(255);
                continue; // 3D Box: the cover as a box with depth
            }
            if (alpha >= 255.0f) {
                // While the box turns over, the cover is squashed sideways.
                int fw = resting_center ? (int) (w * carousel_art_flip_width()) : w;
                int fx0 = x0 + (w - fw) / 2;
                if (!carousel_art_cutout(menu, i)) {   // a cartridge stands on its own, without a box behind it
                    rdpq_set_mode_fill(panel);
                    rdpq_fill_rectangle(fx0, y0, fx0 + fw, y0 + h);
                }
                if (carousel_art_draw(menu, i, fx0, y0, fw, h, 255, resting_center)) {
                    carousel_art_brightness(255);
                    continue; // box art replaces the placeholder text
                }
            } else {
                if (!carousel_art_cutout(menu, i)) {
                    rdpq_set_mode_standard();
                    rdpq_set_prim_color(RGBA32(panel.r, panel.g, panel.b, (int) alpha));
                    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
                    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                    rdpq_fill_rectangle(x0, y0, x0 + w, y0 + h);
                }
                if (carousel_art_draw(menu, i, x0, y0, w, h, (int) alpha, false)) {
                    carousel_art_brightness(255);
                    continue; // box art replaces the placeholder text
                }
            }
            carousel_art_brightness(255);

            // Text can't fade, so a cover that is fading out loses its text early.
            if (!side_covers && away >= 0.5f) {
                continue;
            }

            entry_t *e = &covers->list[i];

            rdpq_text_printf(&(rdpq_textparms_t) {
                .width = w - 16,
                .wrap = WRAP_ELLIPSES,
                .style_id = STL_GRAY,
            }, FNT_DEFAULT, x0 + 8, y0 + 18, "%s", carousel_type_label(e->type));

            rdpq_text_printf(&(rdpq_textparms_t) {
                .width = w - 16,
                .height = h - 40,
                .wrap = WRAP_WORD,
            }, FNT_DEFAULT, x0 + 8, y0 + 44, "%s", display_name(e));
        }
    }

    // The selection ring stays put in the center; covers slide through it.
    // It takes the selected cover's shape, changing smoothly between a wide
    // box and a tall one.
    int ring_w, ring_h;
    carousel_ring_size(menu, covers->selected, &ring_w, &ring_h);
    bool ring_hidden;
    color_t ring_color = carousel_ring_color(menu, covers->selected, t->accent, &ring_hidden);
    // Corner brackets stand a little further above and below the cover than the ring.
    const int ring_pad_y = (options_get(OPTION_SELECTION) == SELECTION_CORNERS) ? 9 : 5;
    if (!ring_hidden && options_get(OPTION_SELECTION) != SELECTION_OFF) {
        carousel_ring(
            cx - ring_w / 2 - 5, CAROUSEL_CENTER_Y - ring_h / 2 - ring_pad_y,
            cx + ring_w / 2 + 5, CAROUSEL_CENTER_Y + ring_h / 2 + ring_pad_y,
            3, carousel_dimmed(ring_color, center_dimmed ? bright : 255)
        );
    }
    if (!center_dimmed) {
        carousel_box_add(&drawn_center, cx - ring_w / 2 - 5, CAROUSEL_CENTER_Y - ring_h / 2 - ring_pad_y, cx + ring_w / 2 + 5, CAROUSEL_CENTER_Y + ring_h / 2 + ring_pad_y);
    }

    // Safe mode reminder, centered between the tabs and the cover.
    if (safe_mode_active()) {
        int top = GAMES_UI_TOPBAR_BOTTOM;
        int bottom = CAROUSEL_CENTER_Y - CAROUSEL_CENTER_H / 2 - 5;
        rdpq_text_print(&(rdpq_textparms_t) {
            .width = screen_w,
            .height = bottom - top,
            .align = ALIGN_CENTER,
            .valign = VALIGN_CENTER,
            .style_id = STL_ORANGE,
        }, FNT_DEFAULT, 0, top, "Safe Mode");
    }

    // Title panel and position bar under the covers.
    const char *title = display_name(selected_entry);
    // Sorted by developer or date: the line under the name starts with which, ahead of the badges.
    const char *detail = (covers->list == menu->browser.list) ? sort_order_detail(menu->browser.directory, selected_entry) : NULL;
    // Game Versions: Grouped. A cover standing for several files says how many.
    int versions = (covers->list == menu->browser.list) ? games_filter_versions(menu, selected_entry) : 1;
    static char lead_text[64];
    if (versions > 1) {
        if (detail) {
            snprintf(lead_text, sizeof(lead_text), "%s  %d versions", detail, versions);
        } else {
            snprintf(lead_text, sizeof(lead_text), "%d versions", versions);
        }
    }
    const char *lead = (versions > 1) ? lead_text : detail;
    bool hack = (covers->list == menu->browser.list) && games_filter_is_hack(selected_entry); // a changed copy of another game
    if (covers->list != menu->browser.list) { // Recent and Favorites: ask the Games list by path
        path_t *selected_path = covers->path(menu, covers->selected);
        hack = games_filter_is_hack_path(menu, path_get(selected_path));
        path_free(selected_path);
    }
    games_ui_title_panel_draw(title, carousel_kind_label(selected_entry->type), lead, hack, game_facts_update(menu));
    const char *letter_from = !detail ? entry_file_name(selected_entry->name) : (options_get(OPTION_SORT_ORDER) == SORT_DEVELOPER) ? detail : " "; // the letter of what the list is sorted by; none for dates
    games_ui_position_draw(letter_from, covers->selected, covers->entries, (covers->list == menu->browser.list) ? games_filter_label() : NULL); // the file name: that is what the list is sorted by
}
#endif

#if BROWSER_CAROUSEL
// Button hints along the bottom: each button in a small box, then what it does.
static void carousel_hints_draw (menu_t *menu) {
    // Upper row: what A does. Lower row: back and options. Right side: the
    // two that are always there.
    int x = GAMES_UI_HINTS_X;

    games_ui_hints_backdrop_draw();

    if (menu->browser.entry) {
        const char *tap = "Info";
        const char *hold = NULL;
        switch (menu->browser.entry->type) {
            case ENTRY_TYPE_DIR: tap = "Open"; break;
            case ENTRY_TYPE_ROM: controls_rom_actions(&tap, &hold); break;
            case ENTRY_TYPE_DISK: tap = "Load"; break;
            case ENTRY_TYPE_IMAGE: tap = "Show"; break;
            case ENTRY_TYPE_TEXT: tap = "View"; break;
            case ENTRY_TYPE_MUSIC: tap = "Play"; break;
            case ENTRY_TYPE_ARCHIVE: tap = "Open"; break;
            default: break;
        }
        x = games_ui_hint_draw(x, 0, "A", tap);
        if (hold) {
            games_ui_hint_draw(x, 0, "Hold", hold);
        }
    }

    x = GAMES_UI_HINTS_X;
    if (folders_tab && !path_is_root(menu->browser.directory)) {
        x = games_ui_hint_draw(x, 1, "B", "Back");
    } else if (!folders_tab && menu->browser.entry && menu->browser.entry->type == ENTRY_TYPE_ROM && features_enabled(FEATURE_BOX_3D)) {
        x = games_ui_hint_draw(x, 1, "B", "Inspect");   // hold B to turn the 3D Box (box_motion.h)
    }
    if (menu->browser.entry) {
        games_ui_hint_draw(x, 1, "Z", "Options");
    }

    games_ui_hint_right_draw(0, "START", "Settings");
    games_ui_hint_right_draw(1, "C", "Scroll ◀▶ Filter ▲▼");
}
#endif

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach(d, NULL);

    // The fade-in. Darkening the whole screen with see-through black made the
    // fade run at half speed, so on the Games tab the background is drawn
    // already darkened and only the parts with something on them are
    // darkened afterwards. Anything that pops up over the screen, or any
    // other case, falls back to darkening everything.
    int fade_dark = intro_fade_dark();
    bool fade_parts = fade_dark > 0 && fade_dark < 255 && !folders_tab && menu->browser.entries > 0
        && !safe_mode_active() && get_ticks_ms() >= note_until_ms
        && entry_context_menu.row_selected < 0 && archive_context_menu.row_selected < 0 && settings_context_menu.row_selected < 0;
    if (fade_parts) {
        theme_background_dim(fade_dark);
    }

    ui_components_background_draw();
    if (fade_parts && !theme_background_dimmed()) {
        theme_background_dim(0); // the player's own picture was drawn instead: don't leave the request waiting
        fade_parts = false;
    }
    if (fade_parts) {
        games_ui_fade_rects(theme_background_draw_part, true); // not darkened twice where things sit straight on it
        carousel_dark = fade_dark; // covers with art are drawn darkened instead
    }

#if BROWSER_CAROUSEL
    if (folders_tab) {
        games_ui_topbar_draw(menu, GAMES_TAB_FOLDERS);
        folders_ui_draw(menu);
    } else {
        games_ui_topbar_draw(menu, GAMES_TAB_GAMES);
        carousel_draw(menu);
        if (menu->browser.entries <= 0) {
            rdpq_text_printf(&(rdpq_textparms_t) {
                .width = DISPLAY_WIDTH - (2 * GAMES_UI_CONTENT_X0),
                .height = 200,
                .align = ALIGN_CENTER,
                .valign = VALIGN_CENTER,
                .wrap = WRAP_WORD,
                .style_id = STL_GRAY,
            }, FNT_DEFAULT, GAMES_UI_CONTENT_X0, 100,
                "No games found.\n\n"
                "Open the Folders tab, go to a folder with games, press Z "
                "and choose \"Add this folder to Games\".");
        }
    }
#else
    ui_components_tabs_common_draw(0);
    ui_components_layout_draw_tabbed();
    ui_components_file_list_draw(menu->browser.list, menu->browser.entries, menu->browser.selected);
#endif

#if BROWSER_CAROUSEL
    carousel_hints_draw(menu);
#else
    const char *action = NULL;

    if (menu->browser.entry) {
        switch (menu->browser.entry->type) {
            case ENTRY_TYPE_DIR: action = "A: Enter"; break;
            case ENTRY_TYPE_ROM: action = controls_rom_hint(); break;
            case ENTRY_TYPE_DISK: action = "A: Load"; break;
            case ENTRY_TYPE_IMAGE: action = "A: Show"; break;
            case ENTRY_TYPE_TEXT: action = "A: View"; break;
            case ENTRY_TYPE_MUSIC: action = "A: Play"; break;
            case ENTRY_TYPE_ARCHIVE: action = "A: Open"; break;
            default: action = "A: Info"; break;
        }
    }

    ui_components_actions_bar_text_draw(
        STL_DEFAULT,
        ALIGN_LEFT, VALIGN_TOP,
        "%s\n"
        "^%02XB: Back^00",
        menu->browser.entries == 0 ? "" : action,
        path_is_root(menu->browser.directory) ? STL_GRAY : STL_DEFAULT
    );

    ui_components_actions_bar_text_draw(
        STL_DEFAULT,
        ALIGN_RIGHT, VALIGN_TOP,
        "^%02XStart: Settings^00\n"
        "^%02XZ:  Options^00",
        menu->browser.entries == 0 ? STL_GRAY : STL_DEFAULT
    );

    // The clock moved to the tab bar at the top.
    ui_components_actions_bar_text_draw(
        STL_DEFAULT,
        ALIGN_CENTER, VALIGN_TOP,
        "C: Fast\n"
        "\n"
    );
#endif

    ui_components_context_menu_draw(menu->browser.archive ? &archive_context_menu : &entry_context_menu);
    ui_components_context_menu_draw(&versions_menu);

    ui_components_context_menu_draw(&settings_context_menu);

    note_draw(); // "Added..." after a folder is added to Games
    if (fade_parts) {
        carousel_fade_rects(intro_fade_rect);
        games_ui_fade_rects(intro_fade_rect, false);
    } else {
        intro_fade_draw(); // the fade-in after the menu starts, over the whole screen
    }
    debug_stats_overlay_draw(); // Performance Overlay, if it is on

    rdpq_detach_show();
}


void view_browser_init (menu_t *menu) {
    games_ui_set_origin(MENU_MODE_BROWSER); // where B on a game's info screen comes back to
    if (menu->browser.select_file) tabs_browser_show_folders(true); // being sent to a particular file: that is the Folders tab's job
    browser_show_tab(menu, tabs_browser_shows_folders());
    if (!folders_tab && !path_is_root(menu->browser.directory)) {
        // The Games tab reads its folders from the top of the card (games_folders.h).
        path_free(menu->browser.directory);
        menu->browser.directory = path_init(menu->storage_prefix, "/");
        menu->browser.valid = false;
    }
    if (!menu->browser.valid) {
        ui_components_context_menu_init(&entry_context_menu);
        ui_components_context_menu_init(&archive_context_menu);
        ui_components_context_menu_init(&settings_context_menu);
        ui_components_context_menu_init(&versions_menu);       // empty, so hidden
        if (load_directory(menu)) {
            path_free(menu->browser.directory);
            menu->browser.directory = path_init(menu->storage_prefix, "");
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Initial directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Error while opening initial directory"
            );
        } else {
            menu->browser.valid = true;
            const char *left = left_on[folders_tab ? 1 : 0];
            if (left[0]) {
                // Back on this tab: the entry it was left on.
                for (int i = 0; i < menu->browser.entries; i++) {
                    if (strcmp(menu->browser.list[i].name, left) == 0) {
                        menu->browser.selected = i;
                        menu->browser.entry = &menu->browser.list[i];
                        break;
                    }
                }
            }
        }
    }

    if (menu->browser.select_file) {
        if (select_file(menu, menu->browser.select_file)) {
            menu->browser.valid = false;
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Target directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Error while navigating to file"
            );
        }
        path_free(menu->browser.select_file);
        menu->browser.select_file = NULL;
    }

    if (menu->browser.reload) {
        menu->browser.reload = false;
        if (reload_directory(menu)) {
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Current directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Error while reloading current directory"
            );
            menu->browser.valid = false;
        }
    }
}

void view_browser_display (menu_t *menu, surface_t *display) {
    process(menu);
    folder_memory_use_key(folders_tab ? NULL : "games:"); // the Games tab keeps its own place
    folder_memory_update(menu); // put the selection back where it was left in this folder

    draw(menu, display);

    // Cover art is fetched here, after the picture has been handed to the graphics chip, so the chip draws while the art loads
    if (!folders_tab && menu->next_mode == MENU_MODE_BROWSER) carousel_art_update(menu, features_enabled(FEATURE_SIDE_COVERS));
    if (!folders_tab && menu->next_mode == MENU_MODE_BROWSER) game_learn_update(menu); // while idle, read the headers of games not known yet

    if (menu->next_mode != MENU_MODE_BROWSER) carousel_art_reset(); // free the cover before another screen loads its own
    if (menu->next_mode != MENU_MODE_BROWSER) folder_memory_flush();
    if (menu->next_mode != MENU_MODE_BROWSER) game_facts_reset(); // favorites may change while away
}
