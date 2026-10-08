#include "utils/fs.h"
#include "../intro.h"
#include "../tabs.h"
#include "views.h"


static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);
    rdpq_detach_show();
}


void view_startup_init (menu_t *menu) {
#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    // FIXME: rather than use a controller button, would it be better to use the cart button?
    JOYPAD_PORT_FOREACH (port) {
        joypad_poll();
        joypad_buttons_t b_held = joypad_get_buttons_held(port);

        if (menu->settings.rom_autoload_enabled && b_held.start) {
            menu->settings.rom_autoload_enabled = false;
            menu->settings.rom_autoload_path = "";
            menu->settings.rom_autoload_filename = "";
            settings_save(&menu->settings);
        }
    }
    if (menu->settings.rom_autoload_enabled) {
        menu->browser.directory = path_init(menu->storage_prefix, menu->settings.rom_autoload_path);
        menu->load.rom_path = path_clone_push(menu->browser.directory, menu->settings.rom_autoload_filename);
        menu->load_pending.rom_file = true;
        menu->next_mode = MENU_MODE_LOAD_ROM;

        return;
    }
#endif
    
    if (menu->settings.first_run) {
        menu->settings.first_run = false;
        settings_save(&menu->settings);
        menu->next_mode = MENU_MODE_CREDITS;
    }
    else {
        menu->next_mode = MENU_MODE_BROWSER;
    }

    if (menu->next_mode == MENU_MODE_BROWSER) tabs_open(menu, tabs_first()); // the menu opens on the player's first tab
    intro_begin(menu); // plays the intro first, if it is wanted
}

void view_startup_display (menu_t *menu, surface_t *display) {
    if (intro_display(menu, display)) {
        return;
    }
    draw(menu, display);
}
