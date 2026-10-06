#include <libdragon.h>
#include <string.h>

#include "cart_load.h"
#include "game_facts.h"
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

void game_facts_reset (void) {
    watched_entry = NULL;
    watched_index = -1;
    watched_hash = 0;
    pending = false;
    known = false;
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

static void lookup (menu_t *menu, entry_t *entry) {
    static rom_info_t info;

    memset(&facts, 0, sizeof(facts));

    path_t *path = path_clone_push(menu->browser.directory, entry->name);

    memset(&info, 0, sizeof(info));
    if (rom_config_load(path, &info) == ROM_OK) {
        facts.players = (int) info.meta.num_players;
        facts.needs_expansion = (info.features.expansion_pak == EXPANSION_PAK_REQUIRED);
        facts.likes_expansion = (info.features.expansion_pak == EXPANSION_PAK_RECOMMENDED) ||
            (info.features.expansion_pak == EXPANSION_PAK_SUGGESTED);
        facts.saves = (rom_info_get_save_type(&info) != SAVE_TYPE_NONE);
        known = true;
    }
    rom_info_free_meta(&info);

    if (known) {
        facts.favorite = is_favorite(menu, path);

        /* The save file sits where the loader would put it. */
        if (facts.saves) {
            path_ext_replace(path, "sav");
            if (menu->settings.use_saves_folder) {
                path_push_subdir(path, SAVE_DIRECTORY_NAME);
            }
            facts.save_found = file_exists(path_get(path));
        }
    }

    path_free(path);
}

const game_facts_t *game_facts_update (menu_t *menu) {
    if (menu->browser.entries <= 0 || menu->browser.selected < 0 || !menu->browser.entry) {
        game_facts_reset();
        return NULL;
    }

    uint64_t now = get_ticks_ms();
    entry_t *entry = menu->browser.entry;
    uint32_t hash = name_hash(entry->name);

    if (entry != watched_entry || menu->browser.selected != watched_index || hash != watched_hash) {
        watched_entry = entry;
        watched_index = menu->browser.selected;
        watched_hash = hash;
        changed_at = now;
        known = false;
        pending = (entry->type == ENTRY_TYPE_ROM);
    }

    if (pending && (now - changed_at) >= SETTLE_TIME_MS) {
        pending = false;
        lookup(menu, entry);
    }

    return known ? &facts : NULL;
}
