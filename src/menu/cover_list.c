/**
 * @file cover_list.c
 * @brief Where the cover row gets its games from.
 */

#include <stddef.h>
#include "cover_list.h"

static const cover_list_t *other = NULL;
static cover_list_t folder;

static path_t *folder_path (menu_t *menu, int index) {
    return path_clone_push(menu->browser.directory, menu->browser.list[index].name);
}

void cover_list_use (const cover_list_t *list) {
    other = list;
}

const cover_list_t *cover_list_current (menu_t *menu) {
    if (other) {
        return other;
    }
    folder.list = menu->browser.list;
    folder.entries = menu->browser.entries;
    folder.selected = menu->browser.selected;
    folder.path = folder_path;
    return &folder;
}

int cover_list_neighbor (menu_t *menu, int offset) {
    const cover_list_t *covers = cover_list_current(menu);
    int index = covers->selected + offset;
    if (index >= 0 && index < covers->entries) {
        return index;
    }
    int away = (offset < 0) ? -offset : offset;
    if (!menu->settings.wrap_file_list_scrolling || covers->selected < 0 || (away * 2) >= covers->entries) {
        return -1;
    }
    return ((index % covers->entries) + covers->entries) % covers->entries;
}
