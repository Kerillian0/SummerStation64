/**
 * @file tabs.c
 * @brief Which tabs are on the tab bar, and in what order.
 */

#include <libdragon.h>
#include "menu_features.h"
#include "menu_options.h"
#include "collections.h"
#include "tabs.h"

static bool folders_wanted = false;
static bool homebrew_wanted = false;
static bool collections_wanted = false;
static bool retro_wanted = false;
static bool hacks_wanted = false;
static games_tab_t pinned_wanted = GAMES_TAB_COUNT;

static const option_t places[] = { OPTION_TAB1, OPTION_TAB2, OPTION_TAB3, OPTION_TAB4 };

const char *tabs_choice_name (int choice) {
    switch (choice) {
        case TAB_CHOICE_GAMES: return "Games";
        case TAB_CHOICE_FAVORITES: return "Favorites";
        case TAB_CHOICE_FOLDERS: return "Folders";
        case TAB_CHOICE_RECENT: return "Recent";
        case TAB_CHOICE_HOMEBREW: return "Homebrew";
        case TAB_CHOICE_COLLECTIONS: return "Collections";
        case TAB_CHOICE_RETRO: return "Retro";
        case TAB_CHOICE_HACKS: return "Hacks";
        case TAB_CHOICE_NONE: return "None";
        default: break;
    }
    int id = choice - TAB_CHOICE_COLLECTION;
    if (id < 0 || id >= COLLECTIONS_IDS) {
        return "";
    }
    /* Once the Games list has been read, only collections that have games (or are the player's own). */
    bool known = (collections_count() > 0) ? collections_exists(id) : (collections_name(id)[0] != '\0');
    return known ? collections_name(id) : "";
}

int tabs_pinned_collection (games_tab_t tab) {
    if (tab < GAMES_TAB_PINNED1 || tab > GAMES_TAB_PINNED4) {
        return -1;
    }
    int id = options_get(places[tab - GAMES_TAB_PINNED1]) - TAB_CHOICE_COLLECTION;
    return (id >= 0 && id < COLLECTIONS_IDS) ? id : -1;
}

const char *tabs_label (games_tab_t tab) {
    static const char *const labels[GAMES_TAB_PINNED1] = { "Games", "Recent", "Favorites", "Folders", "Homebrew", "Collections", "Retro", "Hacks" };
    if (tab >= 0 && tab < GAMES_TAB_PINNED1) {
        return labels[tab];
    }
    int id = tabs_pinned_collection(tab);
    return (id >= 0) ? collections_name(id) : "";
}

void tabs_collection_deleted (int id) {
    for (int i = 0; i < 4; i++) {
        int value = options_get(places[i]);
        int other = value - TAB_CHOICE_COLLECTION;
        if (other == id) {
            options_change(places[i], TAB_CHOICE_NONE);
        } else if (other > id && collections_mine(other - 1) >= 0) {
            options_change(places[i], value - 1);   /* the player's own collections after it move up one */
        }
    }
    options_flush();
}

int tabs_list (games_tab_t *out) {
    bool seen[GAMES_TAB_COUNT] = { false };
    int seen_values[4];
    int count = 0;

    for (int i = 0; i < (int) (sizeof(places) / sizeof(places[0])); i++) {
        games_tab_t tab;
        int value = options_get(places[i]);
        seen_values[i] = value;
        switch (value) {
            case TAB_CHOICE_GAMES: tab = GAMES_TAB_GAMES; break;
            case TAB_CHOICE_FAVORITES: tab = GAMES_TAB_FAVORITES; break;
            case TAB_CHOICE_FOLDERS: tab = GAMES_TAB_FOLDERS; break;
            case TAB_CHOICE_RECENT: tab = GAMES_TAB_RECENT; break;
            case TAB_CHOICE_HOMEBREW: tab = GAMES_TAB_HOMEBREW; break;
            case TAB_CHOICE_COLLECTIONS: tab = GAMES_TAB_COLLECTIONS; break;
            case TAB_CHOICE_RETRO: tab = GAMES_TAB_RETRO; break;
            case TAB_CHOICE_HACKS: tab = GAMES_TAB_HACKS; break;
            default: {
                int id = value - TAB_CHOICE_COLLECTION;
                if (id < 0 || id >= COLLECTIONS_IDS || !collections_name(id)[0]) {
                    continue;
                }
                bool twice = false;     /* the same collection in two places counts once */
                for (int j = 0; j < i; j++) {
                    twice |= (seen_values[j] == value);
                }
                if (twice) {
                    continue;
                }
                tab = (games_tab_t) (GAMES_TAB_PINNED1 + i);
                break;
            }
        }
        if (!seen[tab]) {
            seen[tab] = true;
            out[count++] = tab;
        }
    }

    /* Every place set to None would leave no way around the menu. */
    if (count == 0) {
        out[count++] = GAMES_TAB_GAMES;
        out[count++] = GAMES_TAB_FAVORITES;
        out[count++] = GAMES_TAB_FOLDERS;
    }

    return count;
}

games_tab_t tabs_first (void) {
    games_tab_t tabs[GAMES_TAB_COUNT];
    tabs_list(tabs);
    return tabs[0];
}

games_tab_t tabs_step (games_tab_t from, int direction) {
    games_tab_t tabs[GAMES_TAB_COUNT];
    int count = tabs_list(tabs);

    for (int i = 0; i < count; i++) {
        if (tabs[i] == from) {
            return tabs[(i + direction + count) % count];
        }
    }
    return tabs[0];     /* the screen we are on is not on the bar: go to the first tab */
}

#define TAB_SLIDE_MS    (200)

static int slide_direction = 0;
static bool slide_started = false;
static uint64_t slide_began_ms = 0;
static uint64_t slide_asked_ms = 0;

static menu_mode_t slide_screen;     /* the screen the slide is for */

static bool slide_for_folders;       /* ... and whether that is the Folders list or a row of covers */

void tabs_slide_begin (menu_t *menu, int direction) {
    slide_direction = features_enabled(FEATURE_CAROUSEL_ANIMATION) ? direction : 0;
    slide_for_folders = (menu->next_mode == MENU_MODE_BROWSER && folders_wanted);
    slide_screen = menu->next_mode;
    slide_started = false;
    slide_asked_ms = get_ticks_ms();
}

static float slide_now (bool folders) {
    /* The tab being left draws one more frame after L or R is pressed; it
       stays where it is. */
    if (slide_direction == 0 || folders != slide_for_folders || games_ui_origin() != slide_screen) {
        return 0.0f;
    }
    uint64_t now = get_ticks_ms();
    if (!slide_started) {
        /* Asked for a while ago and never drawn: too late to play it now. */
        if ((now - slide_asked_ms) > 500) {
            slide_direction = 0;
            return 0.0f;
        }
        slide_started = true;
        slide_began_ms = now;
    }
    float progress = (float) (now - slide_began_ms) / TAB_SLIDE_MS;
    if (progress >= 1.0f) {
        slide_direction = 0;
        return 0.0f;
    }
    float left = (1.0f - progress) * (1.0f - progress); /* fast at first, easing into place */
    return slide_direction * left;
}

float tabs_slide (void) {
    return slide_now(false);
}

float tabs_slide_folders (void) {
    return slide_now(true);
}

void tabs_open (menu_t *menu, games_tab_t tab) {
    folders_wanted = (tab == GAMES_TAB_FOLDERS);
    homebrew_wanted = (tab == GAMES_TAB_HOMEBREW);
    collections_wanted = (tab == GAMES_TAB_COLLECTIONS);
    retro_wanted = (tab == GAMES_TAB_RETRO);
    hacks_wanted = (tab == GAMES_TAB_HACKS);
    pinned_wanted = (tabs_pinned_collection(tab) >= 0) ? tab : GAMES_TAB_COUNT;
    switch (tab) {
        case GAMES_TAB_RECENT:
            menu->next_mode = MENU_MODE_HISTORY;
            break;
        case GAMES_TAB_FAVORITES:
            menu->next_mode = MENU_MODE_FAVORITE;
            break;
        default:
            menu->next_mode = MENU_MODE_BROWSER;    /* Games, Folders and the shelves are one screen */
            break;
    }
}

bool tabs_browser_shows_folders (void) {
    return folders_wanted;
}

void tabs_browser_show_folders (bool folders) {
    folders_wanted = folders;
    if (folders) {
        homebrew_wanted = false;
        collections_wanted = false;
        retro_wanted = false;
        hacks_wanted = false;
        pinned_wanted = GAMES_TAB_COUNT;
    }
}

bool tabs_browser_shows_homebrew (void) {
    return homebrew_wanted && !folders_wanted;
}

bool tabs_browser_shows_collections (void) {
    return collections_wanted && !folders_wanted;
}

bool tabs_browser_shows_retro (void) {
    return retro_wanted && !folders_wanted;
}

bool tabs_browser_shows_hacks (void) {
    return hacks_wanted && !folders_wanted;
}

games_tab_t tabs_browser_pinned (void) {
    return folders_wanted ? GAMES_TAB_COUNT : pinned_wanted;
}
