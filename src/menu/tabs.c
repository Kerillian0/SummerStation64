/**
 * @file tabs.c
 * @brief Which tabs are on the tab bar, and in what order.
 */

#include "menu_options.h"
#include "tabs.h"

static bool folders_wanted = false;

const char *tabs_choice_name (int choice) {
    switch (choice) {
        case TAB_CHOICE_GAMES: return "Games";
        case TAB_CHOICE_FAVORITES: return "Favorites";
        case TAB_CHOICE_FOLDERS: return "Folders";
        case TAB_CHOICE_RECENT: return "Recent";
        default: return "None";
    }
}

int tabs_list (games_tab_t *out) {
    static const option_t places[] = { OPTION_TAB1, OPTION_TAB2, OPTION_TAB3, OPTION_TAB4 };
    bool seen[GAMES_TAB_COUNT] = { false };
    int count = 0;

    for (int i = 0; i < (int) (sizeof(places) / sizeof(places[0])); i++) {
        games_tab_t tab;
        switch (options_get(places[i])) {
            case TAB_CHOICE_GAMES: tab = GAMES_TAB_GAMES; break;
            case TAB_CHOICE_FAVORITES: tab = GAMES_TAB_FAVORITES; break;
            case TAB_CHOICE_FOLDERS: tab = GAMES_TAB_FOLDERS; break;
            case TAB_CHOICE_RECENT: tab = GAMES_TAB_RECENT; break;
            default: continue;
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

void tabs_open (menu_t *menu, games_tab_t tab) {
    switch (tab) {
        case GAMES_TAB_RECENT:
            menu->next_mode = MENU_MODE_HISTORY;
            break;
        case GAMES_TAB_FAVORITES:
            menu->next_mode = MENU_MODE_FAVORITE;
            break;
        case GAMES_TAB_FOLDERS:
            folders_wanted = true;
            menu->next_mode = MENU_MODE_BROWSER;
            break;
        default:
            folders_wanted = false;
            menu->next_mode = MENU_MODE_BROWSER;
            break;
    }
}

bool tabs_browser_shows_folders (void) {
    return folders_wanted;
}

void tabs_browser_show_folders (bool folders) {
    folders_wanted = folders;
}
