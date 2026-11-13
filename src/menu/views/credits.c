#include "views.h"
#include "settings_menu.h"
#include "../sound.h"

#ifndef MENU_VERSION
#define MENU_VERSION "Unknown"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "Unknown"
#endif

static sys_version_t sdk_version = {0};
static bool show_oss_lib_info_message = false;

static void process (menu_t *menu) {
    if (menu->actions.back && show_oss_lib_info_message) {
        show_oss_lib_info_message = false;      // B closes the pop-up first
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = settings_menu_back_mode(); // Settings, if opened from there 
    } else if (menu->actions.lz_context) {
        if (show_oss_lib_info_message) {
            show_oss_lib_info_message = false;
        } else {
            show_oss_lib_info_message = true;
        }
        sound_play_effect(SFX_SETTING);
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach(d, NULL);

    ui_components_background_draw();

    ui_components_layout_draw();

    ui_components_main_text_draw(
        STL_DEFAULT,
        ALIGN_CENTER, VALIGN_TOP,
        "MENU INFORMATION"
    );

    ui_components_main_text_draw(
        STL_DEFAULT,
        ALIGN_LEFT, VALIGN_TOP,
        "\n"
        "\n"
        "Menu version: \t%s\n"
        "Build timestamp:  %s\n"
        "libdragon SDK: \t%s%s (%s, %.7s)\n"
        "\n"
        "SummerStation64, a fork of N64FlashcartMenu\n"
        "Latest version: github.com/Kerillian0/SummerStation64\n"
        "\n"
        "N64FlashcartMenu by:\n"
        "\tRobin Jones / NetworkFusion\n"
        "\tMateusz Faderewski / Polprzewodnikowy\n"
        "\tand all its contributors: thank you.\n"
        "Ideas from the N64ever fork (Bjerreman).\n"
        "Made with Claude, an AI model by Anthropic.\n"
        "\nMusic by Eric Matyas www.soundimage.org\n" // this wording, shown here, is a condition of the music's licence (assets/sounds/CREDITS.md)
        "This menu is licensed under the AGPL-3.0 License.\n",
        MENU_VERSION,
        BUILD_TIMESTAMP,
        sdk_version.branch, sdk_version.dirty ? "*" : "",
        sdk_version.commit_date,
        sdk_version.hash
    );

    if (show_oss_lib_info_message) {
        ui_components_messagebox_draw(
            "Libraries used:\n"
            "libdragon (Unlicense)\n"
            "libspng (BSD 2-Clause)\n"
            "minimp3 (CC0 1.0)\n"
            "miniz (MIT), dr_flac (MIT)\n"
            "\n"
            "Sound effects: Lokif (CC0)\n"
            "Genres and players: libretro-database\n"
            "(CC-BY-SA 4.0)\n"
            "\n"
            "Contributors: stewx (title logos,\n"
            "video previews, review scores)\n"
            "\n"
            "SummerStation64 is free, always.\n"
            "Donate: ko-fi.com/polprzewodnikowy\n"
            "and ko-fi.com/networkfusion\n"
            "Help with time: the metadata pack,\n"
            "github.com/n64-tools\n"
        );
    }


    ui_components_actions_bar_text_draw(
        STL_DEFAULT,
        ALIGN_LEFT, VALIGN_TOP,
        "\n"
        "B: Exit"
    );

    ui_components_actions_bar_text_draw(
        STL_DEFAULT,
        ALIGN_RIGHT, VALIGN_TOP,
        "\n"
        "L|Z: OSS Libraries used\n"
    );

    rdpq_detach_show();
}


void view_credits_init (menu_t *menu) {
    sys_get_version(&sdk_version);
}

void view_credits_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
