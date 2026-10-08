/**
 * @file start_menu.c
 * @brief The menu START opens, for the tabs that are not the file browser.
 */

#include "sound.h"
#include "start_menu.h"
#include "ui_components.h"

static void go_to (menu_t *menu, void *arg) {
    menu->next_mode = (menu_mode_t) (arg);
}

/* The same entries as settings_context_menu in views/browser.c. */
static component_context_menu_t start_menu = {
    .list = {
        { .text = "Controller Pak manager", .action = go_to, .arg = (void *) (MENU_MODE_CONTROLLER_PAKFS) },
        { .text = "Menu settings", .action = go_to, .arg = (void *) (MENU_MODE_SETTINGS_EDITOR) },
        { .text = "Time (RTC) settings", .action = go_to, .arg = (void *) (MENU_MODE_RTC) },
        { .text = "Menu information", .action = go_to, .arg = (void *) (MENU_MODE_CREDITS) },
        { .text = "Flashcart information", .action = go_to, .arg = (void *) (MENU_MODE_FLASHCART) },
        { .text = "N64 information", .action = go_to, .arg = (void *) (MENU_MODE_SYSTEM_INFO) },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

void start_menu_init (void) {
    ui_components_context_menu_init(&start_menu);
}

bool start_menu_process (menu_t *menu) {
    if (start_menu.row_selected >= 0 && menu->actions.settings) {
        menu->actions.back = true;  /* START again closes it, like B */
    }
    return ui_components_context_menu_process(menu, &start_menu);
}

void start_menu_show (void) {
    ui_components_context_menu_show(&start_menu);
    sound_play_effect(SFX_SETTING);
}

void start_menu_draw (void) {
    ui_components_context_menu_draw(&start_menu);
}
