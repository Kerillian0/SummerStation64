#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "cart_load.h"
#include "cover_list.h"
#include "game_index.h"
#include "game_facts.h"
#include "ini_parser.h"
#include "path.h"
#include "rom_info.h"
#include "utils/fs.h"

/* How long the selection must rest before the game is looked up. A little
   longer than the cover art's wait, so the two don't land on the same frame. */
#define SETTLE_TIME_MS  (350)

static entry_t *watched_entry = NULL;
static int watched_index = -1;
static uint32_t watched_hash = 0;
static uint64_t changed_at;
static bool pending = false;
static bool known = false;
static game_facts_t facts;

static uint32_t name_hash (const char *name) {
    uint32_t hash = 2166136261u;
    while (*name) {
        hash = (hash ^ (uint8_t) *name++) * 16777619u;
    }
    return hash;
}

static uint32_t saves_folder = 0;   /* which folder's saves are noted (made from its path); 0 = none. See save_exists(). */

void game_facts_reset (void) {
    watched_entry = NULL;
    watched_index = -1;
    watched_hash = 0;
    pending = false;
    known = false;
    saves_folder = 0;   /* saves may be made or deleted while away: read the folder again */
}

/* Facts that never change for a file are remembered, so coming back to a
   game costs no card access. Newest entry replaces the oldest. */
#define REMEMBERED  (32)

typedef struct {
    uint32_t key;       /* hash of the full path, 0 = empty */
    int8_t players;
    bool needs_expansion;
    bool likes_expansion;
    bool saves;
} remembered_t;

static remembered_t remembered[REMEMBERED];
static int remembered_next = 0;

/* How many players, from the metadata pack entry for this game code. */
static int players_from_metadata (menu_t *menu, const char *game_code) {
    char sub[32];
    snprintf(sub, sizeof(sub), "menu/metadata/%c/%c/%c/%c/metadata.ini", game_code[0], game_code[1], game_code[2], game_code[3]);
    path_t *path = path_init(menu->storage_prefix, sub);

    int players = 0;
    if (file_exists(path_get(path))) {
        ini_t *ini = ini_load(path_get(path));
        if (ini) {
            players = ini_get_int(ini, "meta", "num-players", 0);
            ini_free(ini);
        }
    }

    path_free(path);
    return players;
}

static bool is_favorite (menu_t *menu, path_t *rom_path) {
    for (int i = 0; i < FAVORITES_COUNT; i++) {
        bookkeeping_item_t *item = &menu->bookkeeping.favorite_items[i];
        if (item->bookkeeping_type != BOOKKEEPING_TYPE_EMPTY && item->primary_path &&
            strcmp(path_get(item->primary_path), path_get(rom_path)) == 0) {
            return true;
        }
    }
    return false;
}

/* "Is there a save file?" used to ask the card for every game, about 15 ms
   each time. The saves folder is now read once, noting every name in it,
   and the question is answered from those notes. Read again after the
   covers have been left, since saves can be made or deleted meanwhile. */
#define SAVES_NOTED     (512)
static uint32_t saves_names[SAVES_NOTED];
static int saves_count = 0;
static bool saves_too_many = false;

static uint32_t lower_hash (const char *text) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) text; *c; c++) {
        unsigned char ch = (*c >= 'A' && *c <= 'Z') ? (*c + 32) : *c;   /* the card ignores upper and lower case */
        h = (h ^ ch) * 16777619u;
    }
    return h ? h : 1;
}

static bool save_exists (path_t *save_path) {
    path_t *folder = path_clone(save_path);
    path_pop(folder);
    uint32_t key = lower_hash(path_get(folder));

    if (key != saves_folder) {
        saves_folder = key;
        saves_count = 0;
        saves_too_many = false;

        dir_t info;
        int result = dir_findfirst(path_get(folder), &info);
        while (result == 0) {
            if (info.d_type != DT_DIR) {
                if (saves_count < SAVES_NOTED) {
                    saves_names[saves_count++] = lower_hash(info.d_name);
                } else {
                    saves_too_many = true;
                }
            }
            result = dir_findnext(path_get(folder), &info);
        }
        debugf("badges: saves folder read, %d files\n", saves_count);
    }
    path_free(folder);

    if (saves_too_many) {
        return file_exists(path_get(save_path));    /* more than we noted: ask the card */
    }
    uint32_t name = lower_hash(path_last_get(save_path));
    for (int i = 0; i < saves_count; i++) {
        if (saves_names[i] == name) {
            return true;
        }
    }
    return false;
}

static void lookup (menu_t *menu, int index) {
    static rom_info_t info;

    memset(&facts, 0, sizeof(facts));

    path_t *path = cover_list_current(menu)->path(menu, index);
    uint32_t key = name_hash(path_get(path));
    if (key == 0) {
        key = 1;
    }

    remembered_t *found = NULL;
    for (int i = 0; i < REMEMBERED; i++) {
        if (remembered[i].key == key) {
            found = &remembered[i];
            break;
        }
    }

    int64_t size = cover_list_current(menu)->list[index].size;
    const game_index_entry_t *noted = found ? NULL : game_index_find(path_get(path), size);
    if (noted && noted->has_facts) {
        /* Seen in an earlier session: the saved list has the answers. */
        found = &remembered[remembered_next];
        remembered_next = (remembered_next + 1) % REMEMBERED;
        found->key = key;
        found->players = noted->players;
        found->needs_expansion = noted->needs_expansion;
        found->likes_expansion = noted->likes_expansion;
        found->saves = noted->saves;
    }

    if (!found) {
        /* Header and built-in database only: one small read from the card,
           plus the metadata pack entry for the player count. */
        memset(&info, 0, sizeof(info));
        if (rom_info_load_basic(path, &info) == ROM_OK) {
            found = &remembered[remembered_next];
            remembered_next = (remembered_next + 1) % REMEMBERED;
            found->key = key;
            found->players = (int8_t) players_from_metadata(menu, info.game_code);
            found->needs_expansion = (info.features.expansion_pak == EXPANSION_PAK_REQUIRED);
            found->likes_expansion = (info.features.expansion_pak == EXPANSION_PAK_RECOMMENDED) ||
                (info.features.expansion_pak == EXPANSION_PAK_SUGGESTED);
            found->saves = (rom_info_get_save_type(&info) != SAVE_TYPE_NONE);
            game_index_set_facts(path_get(path), size, info.game_code, found->players,
                found->needs_expansion, found->likes_expansion, found->saves);
        }
        rom_info_free_meta(&info);
    }

    if (found) {
        facts.players = found->players;
        facts.needs_expansion = found->needs_expansion;
        facts.likes_expansion = found->likes_expansion;
        facts.saves = found->saves;
        known = true;

        /* These two can change while the menu is running, so they are checked each time. */
        facts.favorite = is_favorite(menu, path);
        if (facts.saves) {
            /* The save file sits where the loader would put it. */
            path_ext_replace(path, "sav");
            if (menu->settings.use_saves_folder) {
                path_push_subdir(path, SAVE_DIRECTORY_NAME);
            }
            facts.save_found = save_exists(path);
        }
    }

    path_free(path);
}

const game_facts_t *game_facts_update (menu_t *menu) {
    const cover_list_t *covers = cover_list_current(menu);
    if (covers->entries <= 0 || covers->selected < 0 || covers->selected >= covers->entries) {
        game_facts_reset();
        return NULL;
    }

    uint64_t now = get_ticks_ms();
    entry_t *entry = &covers->list[covers->selected];
    uint32_t hash = name_hash(entry->name);

    if (entry != watched_entry || covers->selected != watched_index || hash != watched_hash) {
        watched_entry = entry;
        watched_index = covers->selected;
        watched_hash = hash;
        changed_at = now;
        known = false;
        pending = (entry->type == ENTRY_TYPE_ROM);
    }

    if (pending && (now - changed_at) >= SETTLE_TIME_MS) {
        pending = false;
        uint64_t began = get_ticks_ms();
        lookup(menu, covers->selected);
        debugf("badges: lookup took %d ms\n", (int) (get_ticks_ms() - began));
    }

    return known ? &facts : NULL;
}
