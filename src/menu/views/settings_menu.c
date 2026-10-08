#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "../fonts.h"
#include "../folder_memory.h"
#include "../font_choice.h"
#include "../intro.h"
#include "../builtin_themes.h"
#include "../intro_logo.h"
#include "../tabs.h"
#include "../games_ui.h"
#include "../menu_features.h"
#include "../menu_options.h"
#include "../sort_order.h"
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
    ITEM_CHOICE,    /* one of several named values; A moves to the next */
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
    void (*changed) (menu_t *menu);     /* ITEM_SWITCH, ITEM_CHOICE: optional extra work after a change */
    feature_t feature;                  /* ITEM_FEATURE */
    option_t option;                    /* ITEM_CHOICE: which option */
    int choice_count;                   /* ITEM_CHOICE: how many values it has */
    const char *(*choice_name) (int);   /* ITEM_CHOICE: label for a value */
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
#define CHOICES(text, id, count, names, hook, about) \
    { .label = text, .help = about, .type = ITEM_CHOICE, .option = id, .choice_count = count, .choice_name = names, .changed = hook }
#define ACTION(text, func, about) \
    { .label = text, .help = about, .type = ITEM_ACTION, .action = func }
#define INFO(text, func, about) \
    { .label = text, .help = about, .type = ITEM_INFO, .info = func }
#define COUNT(list) ((int) (sizeof(list) / sizeof(list[0])))

/* ---------- extra work after some changes ---------- */

static bool confirm_reset = false;

/* Changes are kept in memory and written to the SD card once: when leaving
   this screen, or after a few seconds without a change (in case the console
   is switched off here). That spares the card a write per button press. */
#define IDLE_SAVE_MS    (5000)

static bool settings_dirty = false;
static bool unsaved = false;
static uint64_t changed_at = 0;

static void save_changes (menu_t *menu) {
    if (!unsaved) {
        return;
    }
    unsaved = false;
    if (settings_dirty) {
        settings_dirty = false;
        settings_save(&menu->settings);
    }
    features_user_flush();
    options_flush();
}

static void reload_browser (menu_t *menu) {
    menu->browser.reload = true;
}

/* The list is rebuilt in the new order; keep the same game selected. */
static void resort_browser (menu_t *menu) {
    menu->browser.reload = true;
    folder_memory_reselect();
}

/* A newly chosen theme shows at once: colors, text and background. */
static void apply_theme (menu_t *menu) {
    (void) menu;
    theme_reload();
    fonts_restyle();
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
    CHOICES("Theme", OPTION_THEME, BUILTIN_THEME_COUNT, builtin_theme_name, apply_theme,
        "The menu's colors and background. From SD Card uses your own theme file (menu/theme/theme.ini), or Sunset if there is none. The others are built in."),
    FEATURE("Cover Art", FEATURE_COVER_ART, "Show box art on the selected cover."),
    FEATURE("Previous/Next Covers", FEATURE_SIDE_COVERS, "Show smaller covers either side of the selected one."),
    FEATURE("See-through Side Covers", FEATURE_SEE_THROUGH_COVERS, "Let the background show through the previous and next covers. Off draws them solid."),
    FEATURE("Cover Slide", FEATURE_CAROUSEL_ANIMATION, "Covers slide into place when you move left or right."),
    FEATURE("Rounded Corners", FEATURE_ROUNDED_CORNERS, "Round off the corners of the tabs, the clock, the title panel, the badges and the band behind the button hints. Off draws them square."),
    FEATURE("Favorite Heart", FEATURE_FAVORITE_HEART, "Mark a favorite game with a heart beside its name. Off shows the word Favorite under the name."),
    FEATURE("Button Icons", FEATURE_BUTTON_ICONS, "Show the buttons in the hints along the bottom in their own shapes and colors. Off shows them as plain boxes."),
    FEATURE("Memory Badge", FEATURE_MEMORY_BADGE, "Show a small Expansion Pak or Jumper Pak under the clock, with the console's memory (8MB or 4MB)."),
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

static const item_t library_items[] = {
    CHOICES("Sort By", OPTION_SORT_ORDER, SORT_COUNT, sort_order_name, resort_browser,
        "Type: folders, then each kind of file. Name: folders, then everything by name. Recently Played: games you played from this folder come first."),
    FEATURE("Hide Game Extensions", FEATURE_HIDE_EXTENSIONS, "Show games without the ending of the file name, such as .z64. Other files keep theirs."),
    FEATURE("Tidy Game Titles", FEATURE_TIDY_TITLES, "Show names like \"Legend of Zelda, The\" as \"The Legend of Zelda\", and underscores as spaces. The files are not renamed."),
    FEATURE("Hide Region Tags", FEATURE_HIDE_TAGS, "Hide the region and version tags in brackets, such as (U) (V1.2) [!]. Two versions of one game then look the same in the list."),
    FEATURE("Count Plays", FEATURE_PLAY_STATS, "Keep count of how often each game is started and when it was last played, shown on its info screen."),
    FEATURE("Remember Position", FEATURE_REMEMBER_SELECTION, "Going back into a folder returns to the game you had selected there, also after playing."),
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

static const item_t tab_items[] = {
    CHOICES("First Tab", OPTION_TAB1, TAB_CHOICE_COUNT, tabs_choice_name, NULL,
        "What the first place on the tab bar holds. The menu opens on this tab. L and R step through the tabs."),
    CHOICES("Second Tab", OPTION_TAB2, TAB_CHOICE_COUNT, tabs_choice_name, NULL,
        "What the second place on the tab bar holds. None leaves it out."),
    CHOICES("Third Tab", OPTION_TAB3, TAB_CHOICE_COUNT, tabs_choice_name, NULL,
        "What the third place on the tab bar holds. None leaves it out."),
    CHOICES("Fourth Tab", OPTION_TAB4, TAB_CHOICE_COUNT, tabs_choice_name, NULL,
        "What the fourth place on the tab bar holds. Choose Recent here to add the games you played last."),
};

static const item_t system_items[] = {
    CHOICES("Character Set", OPTION_FONT, FONT_COUNT, font_choice_name, NULL,
        "Latin Only frees about 700 KB of memory but can't show Japanese names. Auto uses Latin Only without the Expansion Pak, Full with it. Restart the console to apply."),
    INFO("Start Folder", default_folder, "The folder the menu opens in. Change it from Options on the Files screen."),
    CHOICES("Intro", OPTION_INTRO, INTRO_CHOICE_COUNT, intro_choice_name, NULL,
        "The short intro with its tune. On shows it when the console is switched on. Both shows it after RESET as well. Any button skips it. The tune follows the Sound Effects setting."),
    CHOICES("Intro Logo", OPTION_INTRO_LOGO, INTRO_LOGO_COUNT, intro_logo_name, NULL,
        "The look of the spinning logo in the intro. Vaporwave has patterned sides. Classic is the console's own green, blue, red and yellow."),
    CHOICES("Fade In", OPTION_FADE, FADE_COUNT, intro_fade_name, NULL,
        "When the menu starts, the picture comes up from black and the background music rises, over one or two seconds. Off shows the menu at once."),
    FEATURE("Remember Settings Page", FEATURE_REMEMBER_SETTINGS, "Reopen Settings on the page and row you last used, until the console is switched off."),
    ACTION("Reset Settings", ask_reset, "Put the stock settings back to how they were on a fresh install."),
};

static const category_t categories[] = {
    { "Display", display_items, COUNT(display_items) },
    { "Controls", control_items, COUNT(control_items) },
    { "Sound", sound_items, COUNT(sound_items) },
    { "Tabs", tab_items, COUNT(tab_items) },
    { "Library", library_items, COUNT(library_items) },
    { "Files", file_items, COUNT(file_items) },
    { "System", system_items, COUNT(system_items) },
};

/* ---------- state ---------- */

static int category = 0;
static int item = 0;
static bool in_items = false;   /* false: choosing a category, true: inside one */
static int first_row = 0;       /* first setting shown, when a category has more than fit */

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
            settings_dirty = true;
            break;
        }
        case ITEM_FEATURE: {
            /* Default -> On -> Off -> Default */
            int value = features_user_get(it->feature);
            int next = (value == FEATURE_UNSET) ? 1 : (value == 1) ? 0 : FEATURE_UNSET;
            features_user_change(it->feature, next);
            break;
        }
        case ITEM_CHOICE: {
            int next = (options_get(it->option) + 1) % it->choice_count;
            options_change(it->option, next);
            if (it->changed) {
                it->changed(menu);
            }
            break;
        }
        case ITEM_ACTION:
            it->action(menu);
            break;
        case ITEM_INFO:
            break;
    }

    unsaved = true;
    changed_at = get_ticks_ms();
}

static void process (menu_t *menu) {
    if (confirm_reset) {
        if (menu->actions.enter) {
            confirm_reset = false;
            settings_dirty = false; // don't write the old values back over the reset
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
            menu->next_mode = games_ui_origin(); // back to the tab Settings was opened from
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
#define VISIBLE_ROWS    (8)     /* settings shown at once; longer lists scroll */
#define HELP_Y          (ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT) + 4)
#define HELP_HEIGHT     (LAYOUT_ACTIONS_SEPARATOR_Y - 8 - HELP_Y)
#define ROW_PADDING     (6)
#define SCROLLBAR_X     (ITEMS_X + ITEMS_WIDTH + 3)
#define SCROLLBAR_WIDTH (4)

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
                /* Not chosen by the player: show what it comes out as, dimmed.
                   (Usually the theme's value; a launch mode shows Off while
                   the other launch mode is on.) */
                value = features_enabled(it->feature) ? "Default (On)" : "Default (Off)";
                style = STL_GRAY;
            } else {
                value = on_off(user != 0);
            }
            break;
        }
        case ITEM_CHOICE:
            value = it->choice_name(options_get(it->option));
            break;
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

    // Keep the selected setting inside the rows that fit above the description.
    if (!in_items || first_row > item) {
        first_row = in_items ? item : 0;
    } else if (item >= first_row + VISIBLE_ROWS) {
        first_row = item - VISIBLE_ROWS + 1;
    }
    int last_row = first_row + VISIBLE_ROWS;
    if (last_row > cat->count) {
        last_row = cat->count;
    }

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

    // A page with more rows than fit gets a bar showing how far down you are.
    if (cat->count > VISIBLE_ROWS) {
        int track = VISIBLE_ROWS * ROW_HEIGHT;
        int thumb = (track * VISIBLE_ROWS) / cat->count;
        int thumb_y = ROWS_Y + ((track - thumb) * first_row) / (cat->count - VISIBLE_ROWS);
        draw_fill(SCROLLBAR_X, ROWS_Y, SCROLLBAR_X + SCROLLBAR_WIDTH, ROWS_Y + track, theme_get()->tab_inactive);
        draw_fill(SCROLLBAR_X, thumb_y, SCROLLBAR_X + SCROLLBAR_WIDTH, thumb_y + thumb, theme_get()->accent);
    }

    for (int i = first_row; i < last_row; i++) {
        int y = ROWS_Y + ((i - first_row) * ROW_HEIGHT);
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
    confirm_reset = false;

    // Start from the top, unless the player wants to come back to where they were.
    if (!features_enabled(FEATURE_REMEMBER_SETTINGS)) {
        category = 0;
        item = 0;
        in_items = false;
        first_row = 0;
    }
}

void view_settings_menu_display (menu_t *menu, surface_t *display) {
    process(menu);

    if (menu->next_mode != MENU_MODE_SETTINGS_EDITOR || (unsaved && (get_ticks_ms() - changed_at) >= IDLE_SAVE_MS)) {
        save_changes(menu);
    }

    draw(menu, display);
}
