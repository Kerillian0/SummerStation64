#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "../fonts.h"
#include "../menu_features.h"
#include "../settings.h"
#include "../sound.h"
#include "../theme.h"
#include "../ui_components/constants.h"
#include "settings_menu.h"
#include "views.h"

/* ---------- what a row can be ---------- */

typedef enum {
    ITEM_SWITCH,    /* an On/Off value in settings_t */
    ITEM_FEATURE,   /* a menu feature: Default / On / Off */
    ITEM_ACTION,    /* does something when A is pressed */
    ITEM_INFO,      /* shows a value, can't be changed here */
} item_type_t;

typedef struct {
    const char *label;
    const char *help;
    item_type_t type;
    size_t offset;                      /* ITEM_SWITCH: where the bool lives in settings_t */
    bool stock_default;                 /* ITEM_SWITCH: value on a fresh install */
    const char *on_text;                /* ITEM_SWITCH: shown instead of "On" (optional) */
    const char *off_text;               /* ITEM_SWITCH: shown instead of "Off" (optional) */
    void (*changed) (menu_t *menu);     /* ITEM_SWITCH: optional extra work after a change */
    feature_t feature;                  /* ITEM_FEATURE */
    void (*action) (menu_t *menu);      /* ITEM_ACTION */
    const char *(*info) (menu_t *menu); /* ITEM_INFO */
} item_t;

typedef struct {
    const char *label;
    const item_t *items;
    int count;
} category_t;

#define SWITCH(text, field, def, hook, about) \
    { .label = text, .help = about, .type = ITEM_SWITCH, .offset = offsetof(settings_t, field), .stock_default = def, .changed = hook }
#define CHOICE(text, field, def, off_name, on_name, hook, about) \
    { .label = text, .help = about, .type = ITEM_SWITCH, .offset = offsetof(settings_t, field), .stock_default = def, \
      .off_text = off_name, .on_text = on_name, .changed = hook }
#define FEATURE(text, id, about) \
    { .label = text, .help = about, .type = ITEM_FEATURE, .feature = id }
#define ACTION(text, func, about) \
    { .label = text, .help = about, .type = ITEM_ACTION, .action = func }
#define INFO(text, func, about) \
    { .label = text, .help = about, .type = ITEM_INFO, .info = func }
#define COUNT(list) ((int) (sizeof(list) / sizeof(list[0])))

/* ---------- extra work after some changes ---------- */

static bool confirm_reset = false;

static void reload_browser (menu_t *menu) {
    menu->browser.reload = true;
}

static void apply_soundfx (menu_t *menu) {
    sound_use_sfx(menu->settings.soundfx_enabled);
}

static void apply_bgm (menu_t *menu) {
    sound_use_bgm(menu->settings.bgm_enabled);
}

/* PAL60 only exists on PAL consoles; switch the video timing straight away. */
static void apply_pal60 (menu_t *menu) {
    if (get_tv_type() != TV_PAL) {
        menu->settings.pal60_enabled = false;
        return;
    }
    vi_set_timing_preset(menu->settings.pal60_enabled ? &VI_TIMING_PAL60 : &VI_TIMING_PAL);
}

static void remove_background (menu_t *menu) {
    (void) menu;
    ui_components_background_clear();
}

static void ask_reset (menu_t *menu) {
    (void) menu;
    confirm_reset = true;
}

static const char *default_folder (menu_t *menu) {
    return menu->settings.default_directory;
}

/* ---------- the menu itself ---------- */

static const item_t display_items[] = {
    FEATURE("Cover Art", FEATURE_COVER_ART, "Show box art on the selected cover."),
    FEATURE("Previous/Next Covers", FEATURE_SIDE_COVERS, "Show smaller covers either side of the selected one."),
    FEATURE("Cover Slide", FEATURE_CAROUSEL_ANIMATION, "Covers slide into place when you move left or right."),
    FEATURE("Frame Borders", FEATURE_FRAME_BORDERS, "Draw the frame around the screen and the line above the button hints."),
    CHOICE("Video Output", force_progressive_scan, false, "480i", "240p", NULL,
        "480i is sharper but can flicker. 240p is steady with softer text, and suits TVs that struggle with interlaced video. Restart the console to apply."),
    SWITCH("PAL60 Mode", pal60_enabled, false, apply_pal60,
        "PAL consoles only. The picture may go dark if your TV can't show it; to undo that, edit menu/config.ini on the SD card."),
    ACTION("Remove Background", remove_background, "Remove the background picture set from the image viewer."),
};

static const item_t control_items[] = {
    FEATURE("Quick Launch", FEATURE_QUICK_LAUNCH, "Tap A to start a game. Hold A to see its info instead."),
    FEATURE("Hold A To Launch", FEATURE_HOLD_LAUNCH, "Tap A to see a game's info. Hold A to start it."),
    FEATURE("Up/Down Also Scroll", FEATURE_UPDOWN_SCROLL, "Up and down move through the covers too. When this is off, up and down turn the game box over to show its back."),
    SWITCH("Wrap File List", wrap_file_list_scrolling, false, NULL, "Going past the last item jumps back to the first."),
};

static const item_t sound_items[] = {
    SWITCH("Sound Effects", soundfx_enabled, false, apply_soundfx, "Play sounds when moving around the menu."),
    SWITCH("Background Music", bgm_enabled, false, apply_bgm, "Play music in the menu."),
};

static const item_t file_items[] = {
    SWITCH("Show Hidden Files", show_protected_entries, false, reload_browser, "Show files and folders the menu normally hides."),
    SWITCH("Use Saves Folder", use_saves_folder, true, NULL, "Keep game saves in a separate saves folder."),
    SWITCH("Show Saves Folder", show_saves_folder, false, reload_browser, "Show saves folders in the file list."),
    SWITCH("Show Save Files", show_save_files, false, reload_browser, "Show save files in the file list."),
    SWITCH("Show Cheat Files", show_cheat_files, false, reload_browser, "Show cheat files in the file list."),
#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    SWITCH("ROM Loading Bar", loading_progress_bar_enabled, true, NULL, "Show a progress bar while a game loads."),
#else
    SWITCH("Fast Reboot ROM", rom_fast_reboot_enabled, false, NULL, "Press the console's Reset button to restart the last game."),
#endif
};

static const item_t system_items[] = {
    INFO("Start Folder", default_folder, "The folder the menu opens in. Change it from Options on the Files screen."),
    ACTION("Reset Settings", ask_reset, "Put the stock settings back to how they were on a fresh install."),
};

static const category_t categories[] = {
    { "Display", display_items, COUNT(display_items) },
    { "Controls", control_items, COUNT(control_items) },
    { "Sound", sound_items, COUNT(sound_items) },
    { "Files", file_items, COUNT(file_items) },
    { "System", system_items, COUNT(system_items) },
};

/* ---------- state ---------- */

static int category = 0;
static int item = 0;
static bool in_items = false;   /* false: choosing a category, true: inside one */

static bool *switch_value (menu_t *menu, const item_t *it) {
    return (bool *) ((char *) &menu->settings + it->offset);
}

static void change_item (menu_t *menu, const item_t *it) {
    switch (it->type) {
        case ITEM_SWITCH: {
            bool *value = switch_value(menu, it);
            *value = !*value;
            if (it->changed) {
                it->changed(menu);
            }
            settings_save(&menu->settings);
            break;
        }
        case ITEM_FEATURE: {
            /* Default -> On -> Off -> Default */
            int value = features_user_get(it->feature);
            int next = (value == FEATURE_UNSET) ? 1 : (value == 1) ? 0 : FEATURE_UNSET;
            features_user_set(it->feature, next);
            break;
        }
        case ITEM_ACTION:
            it->action(menu);
            break;
        case ITEM_INFO:
            break;
    }
}

static void process (menu_t *menu) {
    if (confirm_reset) {
        if (menu->actions.enter) {
            confirm_reset = false;
            settings_reset_to_defaults();
            menu_show_error(menu, "Reboot N64 to take effect!");
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            confirm_reset = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    const category_t *cat = &categories[category];

    if (!in_items) {
        if (menu->actions.go_up && category > 0) {
            category--;
            item = 0;
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.go_down && category < COUNT(categories) - 1) {
            category++;
            item = 0;
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.enter || menu->actions.go_right) {
            in_items = true;
            item = 0;
            sound_play_effect(SFX_ENTER);
        } else if (menu->actions.back) {
            menu->next_mode = MENU_MODE_BROWSER;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (menu->actions.go_up && item > 0) {
        item--;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && item < cat->count - 1) {
        item++;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        change_item(menu, &cat->items[item]);
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.back || menu->actions.go_left) {
        in_items = false;
        sound_play_effect(SFX_EXIT);
    }
}

/* ---------- drawing ---------- */

#define ROW_HEIGHT      (26)
#define TITLE_Y         (VISIBLE_AREA_Y0 + 10)
#define ROWS_Y          (TITLE_Y + 28)
#define LEFT_X          (VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL)
#define LEFT_WIDTH      (148)
#define DIVIDER_X       (LEFT_X + LEFT_WIDTH + 8)
#define ITEMS_X         (DIVIDER_X + 12)
#define ITEMS_WIDTH     (VISIBLE_AREA_X1 - TEXT_MARGIN_HORIZONTAL - ITEMS_X)
#define HELP_Y          (ROWS_Y + (8 * ROW_HEIGHT))
#define HELP_HEIGHT     (LAYOUT_ACTIONS_SEPARATOR_Y - 8 - HELP_Y)
#define ROW_PADDING     (6)

static void draw_fill (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

/* The pane being used gets a solid bar; the other keeps a thin underline. */
static void draw_selection (int x, int y, int width, bool focused) {
    if (focused) {
        ui_components_box_draw(x, y, x + width, y + ROW_HEIGHT, FILE_LIST_HIGHLIGHT_COLOR);
    } else {
        draw_fill(x, y + ROW_HEIGHT - 2, x + width, y + ROW_HEIGHT, theme_get()->accent);
    }
}

static void draw_text (int x, int y, int width, rdpq_align_t align, menu_font_style_t style, const char *text) {
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width - (ROW_PADDING * 2),
        .height = ROW_HEIGHT,
        .align = align,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES,
        .style_id = style,
    }, FNT_DEFAULT, x + ROW_PADDING, y, "%s", text);
}

static const char *on_off (bool on) {
    return on ? "On" : "Off";
}

static const char *switch_text (const item_t *it, bool on) {
    const char *text = on ? it->on_text : it->off_text;
    return text ? text : on_off(on);
}

static void draw_item (menu_t *menu, const item_t *it, int y) {
    const char *value = "";
    menu_font_style_t style = STL_DEFAULT;

    switch (it->type) {
        case ITEM_SWITCH:
            value = switch_text(it, *switch_value(menu, it));
            break;
        case ITEM_FEATURE: {
            int user = features_user_get(it->feature);
            if (user == FEATURE_UNSET) {
                /* Not chosen by the player: show what the theme gives, dimmed. */
                value = features_profile_default(it->feature) ? "Default (On)" : "Default (Off)";
                style = STL_GRAY;
            } else {
                value = on_off(user != 0);
            }
            break;
        }
        case ITEM_ACTION:
            break;
        case ITEM_INFO:
            value = it->info(menu);
            style = STL_GRAY;
            break;
    }

    if (it->type == ITEM_INFO) {
        /* Long values (folder paths) get most of the row. */
        draw_text(ITEMS_X, y, 130, ALIGN_LEFT, STL_DEFAULT, it->label);
        draw_text(ITEMS_X + 130, y, ITEMS_WIDTH - 130, ALIGN_RIGHT, style, value);
    } else {
        draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
        draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_RIGHT, style, value);
    }
}

static void draw_help (menu_t *menu, const item_t *it) {
    const char *note = "";
    char stock[32];

    if (it->type == ITEM_SWITCH) {
        snprintf(stock, sizeof(stock), "\nStock value: %s", switch_text(it, it->stock_default));
        note = stock;
    } else if (it->type == ITEM_FEATURE) {
        if (!features_available(it->feature)) {
            note = "\nNeeds the Expansion Pak.";
        } else {
            note = "\nDefault follows the theme. Press A to cycle Default, On, Off.";
        }
    }

    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = ITEMS_WIDTH - (ROW_PADDING * 2),
        .height = HELP_HEIGHT,
        .wrap = WRAP_WORD,
        .style_id = STL_GRAY,
    }, FNT_DEFAULT, ITEMS_X + ROW_PADDING, HELP_Y, "%s%s", it->help, note);
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach(d, NULL);

    ui_components_background_draw();

    ui_components_layout_draw();

    const category_t *cat = &categories[category];

    draw_text(LEFT_X, TITLE_Y, LEFT_WIDTH, ALIGN_LEFT, STL_GRAY, "SETTINGS");
    draw_text(ITEMS_X, TITLE_Y, ITEMS_WIDTH, ALIGN_LEFT, STL_GRAY, cat->label);

    if (features_enabled(FEATURE_FRAME_BORDERS)) {
        draw_fill(DIVIDER_X, ROWS_Y, DIVIDER_X + 2, LAYOUT_ACTIONS_SEPARATOR_Y - 8, theme_get()->border);
    }

    for (int i = 0; i < COUNT(categories); i++) {
        int y = ROWS_Y + (i * ROW_HEIGHT);
        if (i == category) {
            draw_selection(LEFT_X, y, LEFT_WIDTH, !in_items);
        }
        draw_text(LEFT_X, y, LEFT_WIDTH, ALIGN_LEFT, STL_DEFAULT, categories[i].label);
    }

    for (int i = 0; i < cat->count; i++) {
        int y = ROWS_Y + (i * ROW_HEIGHT);
        if (in_items && i == item) {
            draw_selection(ITEMS_X, y, ITEMS_WIDTH, true);
        }
        draw_item(menu, &cat->items[i], y);
    }

    if (in_items) {
        draw_help(menu, &cat->items[item]);
    }

    if (in_items) {
        const item_t *it = &cat->items[item];
        ui_components_actions_bar_text_draw(
            STL_DEFAULT,
            ALIGN_LEFT, VALIGN_TOP,
            "%s\n"
            "B: Back",
            (it->type == ITEM_ACTION) ? "A: Select" : (it->type == ITEM_INFO) ? "" : "A: Change"
        );
    } else {
        ui_components_actions_bar_text_draw(
            STL_DEFAULT,
            ALIGN_LEFT, VALIGN_TOP,
            "A: Open\n"
            "B: Back"
        );
    }

    if (confirm_reset) {
        ui_components_messagebox_draw(
            "Reset settings?\n\n"
            "A: Yes, B: Back"
        );
    }

    rdpq_detach_show();
}

void view_settings_menu_init (menu_t *menu) {
    (void) menu;
    category = 0;
    item = 0;
    in_items = false;
    confirm_reset = false;
}

void view_settings_menu_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
