#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "../assistant.h"
#include "../assistant_ask.h"
#include "../cover_clip.h"
#include "../fonts.h"
#include "../folder_memory.h"
#include "../font_choice.h"
#include "../intro.h"
#include "../game_start.h"
#include "../builtin_themes.h"
#include "../card_themes.h"
#include "../intro_logo.h"
#include "../art_tint.h"
#include "../games_folders.h"
#include "../debug_stats.h"
#include "../tabs.h"
#include "../games_ui.h"
#include "../games_filter.h"
#include "../region_pick.h"
#include "../screenshot.h"
#include "../clip.h"
#include "../art_report.h"
#include "../hidden_games.h"
#include "../box_motion.h"
#include "../carousel_art.h"
#include "../memory_note.h"
#include "../menu_features.h"
#include "../menu_options.h"
#include "../sort_order.h"
#include "../settings.h"
#include "../sound.h"
#include "../theme.h"
#include "../theme_editor.h"
#include "../tv_patterns.h"
#include "../pad_test.h"
#include "../animations.h"
#include "../avatar_pick.h"
#include "../avatars.h"
#include "../letter_wheel.h"
#include "../name_keyboard.h"
#include "../profiles.h"
#include "../profile_pick.h"
#include "../video_preview.h"
#include "../volume.h"
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
    ITEM_SLIDER,    /* a number from 0 to VOLUME_STEPS in an option; left and right move it */
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

typedef struct {
    const char *label;
    const category_t *pages;
    int count;
} group_t;

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
#define ACTION_INFO(text, func, info_func, about) \
    { .label = text, .help = about, .type = ITEM_ACTION, .action = func, .info = info_func }
#define INFO(text, func, about) \
    { .label = text, .help = about, .type = ITEM_INFO, .info = func }
#define SLIDER(text, id, hook, about) \
    { .label = text, .help = about, .type = ITEM_SLIDER, .option = id, .changed = hook }
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
    card_themes_note_choice();
    theme_reload();
    sound_reload_files();   /* the theme's own sounds, or the menu's */
    fonts_restyle();
}

static const char *notice;  /* a note over Settings: defined with the profile rows below */

static void open_ask (menu_t *menu) {
    (void) menu;
    assistant_ask_open(false);
}

/* Fairy Name: typed with the keyboard or dial; an empty name gives it back its own. */
static void fairy_named (menu_t *menu, const char *name) {
    (void) menu;
    if (name) {
        char kept[ASSISTANT_NAME_MAX + 1];
        snprintf(kept, sizeof(kept), "%s", name);
        assistant_rename(kept);
        sound_play_effect(SFX_SETTING);
    }
}

static void rename_fairy (menu_t *menu) {
    (void) menu;
    letter_wheel_name_begin("Fairy's name", assistant_name(), fairy_named);
}

static const char *fairy_name_info (menu_t *menu) {
    (void) menu;
    return assistant_name();
}

static void open_sub_page (const category_t *page);    /* with the page state, further down */

static const char *ask_info (menu_t *menu) {
    (void) menu;
    return assistant_name();    /* who answers */
}

static void reset_assistant (menu_t *menu) {
    (void) menu;
    assistant_reset();
    notice = "The assistant will start its tips over.";
}

static void open_theme_editor (menu_t *menu) {
    (void) menu;
    theme_editor_open();
}

static void new_theme (menu_t *menu) {
    (void) menu;
    theme_editor_new();
}

static void open_test_patterns (menu_t *menu) {
    (void) menu;
    tv_patterns_open();
}

static void open_pad_test (menu_t *menu) {
    (void) menu;
    pad_test_open();
}

static void apply_video_previews (menu_t *menu) {
    video_preview_enable(menu->settings.video_previews_enabled);
}

static void apply_soundfx (menu_t *menu) {
    sound_use_sfx(menu->settings.soundfx_enabled);
}

static void apply_volume (menu_t *menu) {
    (void) menu;
    volume_apply();
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

/* Screens of the stock menu opened from here come back here (settings_menu_back_mode()). */
static bool tool_open = false;

static void open_tool (menu_t *menu, menu_mode_t mode) {
    tool_open = true;
    menu->next_mode = mode;
}
static void open_credits (menu_t *menu) { open_tool(menu, MENU_MODE_CREDITS); }
static void open_flashcart_info (menu_t *menu) { open_tool(menu, MENU_MODE_FLASHCART); }
static void open_system_info (menu_t *menu) { open_tool(menu, MENU_MODE_SYSTEM_INFO); }
static void open_controller_paks (menu_t *menu) { open_tool(menu, MENU_MODE_CONTROLLER_PAKFS); }
static void open_clock (menu_t *menu) { open_tool(menu, MENU_MODE_RTC); }

menu_mode_t settings_menu_back_mode (void) {
    return tool_open ? MENU_MODE_SETTINGS_EDITOR : MENU_MODE_BROWSER;
}

static void ask_reset (menu_t *menu) {
    (void) menu;
    confirm_reset = true;
}

static const char *default_folder (menu_t *menu) {
    return menu->settings.default_directory;
}

/* ---------- the menu itself ---------- */

/* ---------- profiles ---------- */

static int confirm_delete = -1;     /* the profile asked about, or -1 */
static const char *notice = NULL;     /* a short message shown over Settings until any button is pressed */
static int renaming = -1;           /* the profile being renamed, or -1 for a new one */
static char profiles_text[32];

static const char *playing_info (menu_t *menu) {
    (void) menu;
    return profiles_name(profiles_current());
}

static void next_profile (menu_t *menu) {
    if (profiles_count() < 2) {
        notice = "Add a profile first, with New Profile.";   /* stays on Settings (the error screen went back to the tabs) */
        return;
    }
    profiles_switch((profiles_current() + 1) % profiles_count());
    profile_pick_refresh(menu);
}

static const char *new_profile_info (menu_t *menu) {
    (void) menu;
    snprintf(profiles_text, sizeof(profiles_text), "%d of %d", profiles_count(), PROFILES_MAX);
    return profiles_text;
}

static void profile_named (menu_t *menu, const char *name) {
    if (name && name[0]) {
        if (renaming >= 0) {
            profiles_rename(renaming, name);
        } else if (profiles_add(name)) {
            profiles_switch(profiles_count() - 1);  /* the new player is the one setting it up */
            profile_pick_refresh(menu);
            avatar_pick_open(profiles_current());   /* and picks a picture next */
        }
        sound_play_effect(SFX_SETTING);
    }
}

static void new_profile (menu_t *menu) {
    if (profiles_count() >= PROFILES_MAX) {
        notice = "There are four profiles already.";
        return;
    }
    renaming = -1;
    letter_wheel_name_begin("New profile", NULL, profile_named);
}

static void rename_profile (menu_t *menu) {
    (void) menu;
    renaming = profiles_current();
    letter_wheel_name_begin("Rename", profiles_name(renaming), profile_named);
}

static const char *color_names[PROFILE_COLORS] = { "Blue", "Red", "Green", "Yellow" };

static const char *color_info (menu_t *menu) {
    (void) menu;
    return color_names[profiles_color_index(profiles_current())];
}

static void next_color (menu_t *menu) {
    (void) menu;
    profiles_next_color(profiles_current());
}

static const char *picture_info (menu_t *menu) {
    (void) menu;
    return avatars_label(profiles_picture(profiles_current()));
}

static void choose_picture (menu_t *menu) {
    (void) menu;
    avatar_pick_open(profiles_current());
}

static const char *shape_name (int shape) {
    return avatars_shape_name(shape);
}

static void shape_changed (menu_t *menu) {
    (void) menu;
    avatars_shape_changed();    /* each picture is cut to the new shape when next drawn */
}

static void delete_profile (menu_t *menu) {
    if (profiles_current() == 0) {
        notice = "The first profile can't be deleted: it keeps the saves the menu always had.";
        return;
    }
    confirm_delete = profiles_current();
}

static char games_folders_text[16];

static const char *memory_note_info (menu_t *menu) {
    (void) menu;
    return memory_note_value();
}

static const char *games_folders_info (menu_t *menu) {
    (void) menu;
    int n = games_folders_count();
    snprintf(games_folders_text, sizeof(games_folders_text), n == 0 ? "Start folder" : "Start + %d", n);
    return games_folders_text;
}

static void forget_games_folders (menu_t *menu) {
    games_folders_clear();
    menu->browser.valid = false; /* the Games tab reads its list again */
    sound_play_effect(SFX_SETTING);
}

static char hidden_text[16];

static const char *hidden_info (menu_t *menu) {
    (void) menu;
    int n = hidden_games_count();
    snprintf(hidden_text, sizeof(hidden_text), n == 0 ? "None" : "%d", n);
    return hidden_text;
}

static void show_hidden_games (menu_t *menu) {
    hidden_games_clear();
    menu->browser.reload = true;    // the Games list reads them again
}

static const char *clip_status_info (menu_t *menu) {
    return clip_status(menu);
}

static const char *art_report_info (menu_t *menu) {
    (void) menu;
    return art_report_status();
}

static void make_art_report (menu_t *menu) {
    art_report_start(menu);
}

/* A new Preferred Region: every game's own choice of copy is forgotten. */
static void change_preferred_region (menu_t *menu) {
    region_pick_forget_all();
    resort_browser(menu);
}

static const item_t theme_items[] = {
    CHOICES("Theme", OPTION_THEME, THEME_CHOICE_COUNT, card_themes_choice_name, apply_theme,
        "The menu's colors and background: a built-in theme, or one of yours from the SD card, each in its own folder in menu/themes (up to 32). From SD Card is the older menu/theme/theme.ini, or Midnight if there is none."),
    ACTION("Edit Theme", open_theme_editor,
        "Change the theme in use: its colors, background and pattern, seen on the whole menu as you go. Save it as a theme of your own in menu/themes, under a name you choose."),
    ACTION("New Theme", new_theme,
        "Make a theme from scratch: pick a look to start from (a built-in one or plain dark), then change it the same way as Edit Theme and save it under a name you choose."),
    CHOICES("Animations", OPTION_ANIMATIONS, ANIMATIONS_COUNT, animations_name, NULL,
        "Snappy keeps the menu quick and plain. Showy adds movement everywhere: covers, names and badges arrive in turn, the cover flies into Game info and back, highlights and tabs glide, pop-ups open and close, Settings rises in, and the player cards lift. Any button press finishes an animation at once."),
    FEATURE("Rounded Corners", FEATURE_ROUNDED_CORNERS, "Round off the corners of the tabs, the clock, the title panel, the badges and the band behind the button hints. Off draws them square."),
    FEATURE("Button Icons", FEATURE_BUTTON_ICONS, "Show the buttons in the hints along the bottom in their own shapes and colors. Off shows them as plain boxes."),
    CHOICES("Tint Ring From Art", OPTION_RING_TINT, RING_TINT_COUNT, art_tint_name, NULL,
        "Color the ring round the cover to match the cover. Game Info does it on a game's info screen; Everywhere also on the rows of covers. Off uses the theme's accent color."),
    ACTION("Remove Background", remove_background, "Remove the background picture set from the image viewer."),
};

static const item_t cover_items[] = {
    FEATURE("Cover Art", FEATURE_COVER_ART, "Show box art on the selected cover."),
    CHOICES("Show Games As", OPTION_COVER_STYLE, COVER_STYLE_COUNT, carousel_art_style_name, NULL,
        "Boxes shows each game's box art. Cartridges shows its cartridge instead, for games the box art pack has one for; the others keep their box."),
    FEATURE("Previous/Next Covers", FEATURE_SIDE_COVERS, "Show smaller covers either side of the selected one."),
    FEATURE("See-through Side Covers", FEATURE_SEE_THROUGH_COVERS, "Let the background show through the previous and next covers. Off draws them solid."),
    FEATURE("Angled Side Covers", FEATURE_ANGLED_COVERS, "Turn the previous and next covers away toward the sides, like boxes on a shelf. Needs Previous/Next Covers on."),
    CHOICES("Selection Marker", OPTION_SELECTION, SELECTION_COUNT, games_ui_selection_name, NULL,
        "How the selected cover is marked on the rows of covers: Corners puts a bracket at each corner, Ring a frame all round it, Off leaves it unmarked."),
    FEATURE("Trim Black Edges", FEATURE_TRIM_EDGES, "Box art from the SD card loses the thin black frame many scans have, so the picture reaches the box's edges. Only near-black lines across the whole edge, a few pixels at most."),
    FEATURE("Cover Slide", FEATURE_CAROUSEL_ANIMATION, "Covers slide into place when you move left or right."),
};

static const item_t box_items[] = {
    FEATURE("3D Box", FEATURE_BOX_3D, "Show the selected cover as a box with depth, turned slightly, with the top of the box from its box art where there is one."),
    FEATURE("3D Side Covers", FEATURE_SIDE_BOXES, "Draw the previous and next covers as boxes too, turned toward the middle when Angled Side Covers is on. Needs 3D Box and Previous/Next Covers on."),
    CHOICES("Box Hover", OPTION_BOX_HOVER, BOX_HOVER_COUNT, box_motion_hover_name, NULL,
        "How the 3D Box moves while it waits: Float bobs it up and down, Sway turns it gently from side to side. Hold B and use the stick or D-Pad to turn it yourself."),
    FEATURE("Fill Box Edges", FEATURE_FILL_BOX_EDGES, "On a 3D box missing its top label or a side picture, show the edge picture it does have there too, turned to fit. Off leaves those faces plain."),
};

static const item_t badge_items[] = {
    FEATURE("Favorite Heart", FEATURE_FAVORITE_HEART, "Mark a favorite game with a heart beside its name. Off shows the word Favorite under the name."),
    FEATURE("Memory Badge", FEATURE_MEMORY_BADGE, "Show a small Expansion Pak or Jumper Pak under the clock, with the console's memory (8MB or 4MB)."),
    FEATURE("Controller Marks", FEATURE_CONTROLLER_MARKS, "With two or more controllers plugged in, show the four controller ports under the clock in place of the Memory Badge, lit for each one in use. C-up / C-down on the Games tab then also offer Party: the games for that many players."),
    CHOICES("Accessory Badges", OPTION_PAK_BADGES, PAK_BADGES_COUNT, games_ui_pak_badges_name, NULL,
        "Show which accessories a game uses (Controller Pak, Rumble Pak, Transfer Pak) under its name. Colored puts each in its own colored box, like Save found; Plain shows them in grey. Known for most games from the menu's built-in list."),
    FEATURE("Dump Check Marks", FEATURE_DUMP_MARKS, "A green check for a known good dump, an amber ? for a copy that differs from it, a red X for a bad dump. Needs menu/verified.txt from the Card Prep page."),
    FEATURE("Review Scores", FEATURE_REVIEW_SCORES, "Show a game's review score out of 10 at the right of its name under the covers, and in a box on its info screen: green from 7.5, yellow from 5.0, red below. Also adds Review Score to Sort By. Hacks show none. The scores come from menu/reviewscores.txt on the SD card: none come with the menu. A game the menu hasn't looked at yet shows its score after a moment, and sorts with the unscored ones until the list is sorted again."),
    FEATURE("Scroll Long Names", FEATURE_NAME_SCROLL, "On the rows of covers, a name too long for the panel under them slides to its end and back after 2 seconds. Off cuts it short with ..."),
};

static const item_t game_info_items[] = {
    FEATURE("Scroll Descriptions", FEATURE_DESC_SCROLL, "On Game info, a long description drifts up slowly after 10 seconds; the D-Pad or stick scrolls it by hand. Off cuts it short with ..."),
    SWITCH("Video Previews", video_previews_enabled, true, apply_video_previews, "On Game info, a short clip of the game plays in the cover's place after a moment, for games with a preview.h264 beside their box art on the SD card. Needs the Expansion Pak."),
    CHOICES("Clips on Covers", OPTION_COVER_CLIPS, 2, cover_clip_name, NULL,
        "With Video Previews on, the clip also plays on the rows of covers (Games, Recent, Favorites, collections) once a game's cover has rested for 2 seconds. Any button brings the cover back."),
    FEATURE("Advanced Game Options", FEATURE_ADVANCED_OPTIONS, "Game info's Options (R) also offers Set CIC Type, Set Save Type, Set TV Type, the Datel Code Editor and Clear RDRAM on boot. For games that need them; most never do. Choices already made stay in use when this is off."),
};

static void open_fairy_look_row (menu_t *menu);     /* with the fairy's rows, further down */

static const item_t assistant_items[] = {
    CHOICES("Assistant", OPTION_ASSISTANT, ASSISTANT_COUNT, assistant_often_name, NULL,
        "A fairy that flies in now and then with a tip, a joke or a word about your games. Leave the menu alone for a minute and it stays to chat. Any button sends it away. Rarely, Now and Then or Chatty: how often it comes by itself; Chatty also has a word about every game you rest on for 5 seconds. Off: never."),
    CHOICES("Assistant Mood", OPTION_ASSISTANT_MOOD, ASSISTANT_MOOD_COUNT, assistant_mood_name, NULL,
        "Eager is cheerful and keen to help. Snarky gives the same advice with an attitude."),
    ACTION_INFO("Fairy Look...", open_fairy_look_row, ask_info,
        "The fairy itself: its name, which fairy it is, its colour and how it moves. A opens its page, with the fairy shown on it."),
    ACTION_INFO("Ask a Question", open_ask, ask_info,
        "Questions about the menu, answered by the assistant: getting started, finding games, box art, themes, saves, hacks, 64DD, cheats and trouble. Works with the assistant on or off."),
    ACTION("Reset Assistant Tips", reset_assistant,
        "The assistant says each tip once, and skips tips about things you already use. This lets it say the ones it has said again."),
};

/* How the fairy looks; the page shows it perched on the help panel. */
static const item_t fairy_items[] = {
    ACTION_INFO("Fairy Name", rename_fairy, fairy_name_info,
        "Give the fairy a name of your own (each fairy keeps its own name). Erase it all and keep the empty name to give it back its usual one."),
    CHOICES("Character", OPTION_ASSISTANT_SKIN, ASSISTANT_SKIN_COUNT, assistant_skin_name, NULL,
        "Which fairy the assistant is: Vee, the menu's own, or another one built into the menu. A fairy talks about its own games when you rest on them."),
    CHOICES("Fairy Color", OPTION_ASSISTANT_COLOR, ASSISTANT_COLOR_COUNT, assistant_color_name, NULL,
        "The fairy's colour: the glow behind it, the edge of its text box, its sparkles and its name. Vee itself takes the colour too. Its Own is each fairy's usual colour; Theme Accent follows the theme."),
    CHOICES("Arrival", OPTION_ASSISTANT_ARRIVAL, ASSISTANT_ARRIVAL_COUNT, assistant_arrival_name, NULL,
        "How the fairy comes and goes. Fly In: from the right, and off up and away. Sparkle: it pops out of a burst of sparkles. Fade: it appears and fades away. Drop In: it drops from above with a little bounce. The fairy here shows it."),
    CHOICES("Idle Motion", OPTION_ASSISTANT_IDLE, ASSISTANT_IDLE_COUNT, assistant_idle_name, NULL,
        "How the fairy moves while it talks. Bob: gently up and down. Still: it stays put. Wander: a slow figure-eight round its perch. Bounce: little hops."),
    CHOICES("Glow Pulse", OPTION_ASSISTANT_GLOW, ASSISTANT_GLOW_COUNT, assistant_glow_name, NULL,
        "The glow behind the fairy can breathe, slowly growing brighter and dimmer. Off keeps it steady."),
};
static const category_t fairy_page = { "Fairy Look", fairy_items, COUNT(fairy_items) };

/* A row that opens another page's rows in its place (Look > Assistant >
   Fairy Look...); B goes back to the row it was opened from. */
static const category_t *sub_page = NULL;
static int sub_from_item = 0;

static void open_fairy_look_row (menu_t *menu) {
    (void) menu;
    open_sub_page(&fairy_page);
}

static const item_t startup_items[] = {
    CHOICES("Intro", OPTION_INTRO, INTRO_CHOICE_COUNT, intro_choice_name, NULL,
        "The short intro with its tune. On shows it when the console is switched on. Both shows it after RESET as well. Any button skips it. The tune follows the Sound Effects setting."),
    CHOICES("Intro Logo", OPTION_INTRO_LOGO, INTRO_LOGO_COUNT, intro_logo_name, NULL,
        "The look of the spinning logo in the intro. Vaporwave has patterned sides. Classic is the console's own green, blue, red and yellow. Off leaves it out."),
    CHOICES("Intro Title", OPTION_INTRO_TITLE, INTRO_TITLE_COUNT, intro_title_name, NULL,
        "The menu's name under the logo in the intro. Picture is the drawn title, Text the name in plain letters (it uses no extra memory), Off leaves it out. With the logo or the title off, the other sits in the middle."),
    CHOICES("Fade In", OPTION_FADE, FADE_COUNT, intro_fade_name, NULL,
        "When the menu starts, the picture comes up from black and the background music rises, over one or two seconds. Off shows the menu at once."),
    CHOICES("Game Start", OPTION_GAME_START, GAME_START_COUNT, game_start_name, NULL,
        "What shows while a game starts. Box Opens: the box opens and the cartridge comes out, spins and snaps into place while the game loads. Loading Bar is the plain bar."),
};

static const item_t library_items[] = {
    CHOICES("Sort By", OPTION_SORT_ORDER, SORT_COUNT, sort_order_name, resort_browser,
        "Type: folders, then each kind of file. Name: by name. Recently Played: games you played come first. Release Date, Developer: oldest first, or by who made the game; the menu looks these up while the Games tab is left alone. Recently Added: the games the menu saw most recently first. Review Score (with Review Scores on, in Display): the highest scored first, then the games with no score."),
    CHOICES("Game Versions", OPTION_GAME_VERSIONS, GAME_VERSIONS_COUNT, games_filter_versions_name, resort_browser,
        "Separate shows every file as its own cover. Grouped shows one cover per game: its hacks and other copies go under it, and A lists them to choose from. The Hacks filter still shows every hack."),
    CHOICES("Region Copies", OPTION_REGION_COPIES, REGION_COPIES_COUNT, region_pick_copies_name, resort_browser,
        "One Cover shows a game kept in several regions once under All, and A lists the copies to choose from. Start another region's copy three times in a row and it becomes that game's cover. The region filters still show each copy."),
    CHOICES("Preferred Region", OPTION_PREFERRED_REGION, PREFERRED_COUNT, region_pick_preferred_name, change_preferred_region,
        "Whose copy gives a game's cover with Region Copies on One Cover. Changing it also forgets each game's own choice."),
    CHOICES("Homebrew on Games", OPTION_HOMEBREW_ON_GAMES, HOMEBREW_ON_GAMES_COUNT, games_filter_homebrew_name, resort_browser,
        "Hide keeps homebrew off the Games tab and its filters, on the Homebrew tab only (add it in Settings > Games > Tabs). Folders, Recent, Favorites and your own collections still show it."),
    CHOICES("Hacks on Games", OPTION_HACKS_ON_GAMES, HACKS_ON_GAMES_COUNT, games_filter_homebrew_name, resort_browser,
        "Hide keeps ROM hacks off the Games tab, on the Hacks tab (add it in Settings > Games > Tabs) and the Hacks step only. Folders, Recent, Favorites and your own collections still show them."),
};

static const item_t name_items[] = {
    FEATURE("Hide Game Extensions", FEATURE_HIDE_EXTENSIONS, "Show games without the ending of the file name, such as .z64, and without the [!] good-dump marks and revision numbers, such as (V1.1). Other files keep theirs."),
    FEATURE("Tidy Game Titles", FEATURE_TIDY_TITLES, "Show names like \"Legend of Zelda, The\" as \"The Legend of Zelda\", and underscores as spaces. The files are not renamed."),
    FEATURE("Hide Region Tags", FEATURE_HIDE_TAGS, "Hide the region and version tags in brackets, such as (U) (V1.2) [!]. Two versions of one game then look the same in the list. A game's list of versions keeps the region tags."),
};

static const item_t folder_items[] = {
    INFO("Game Folders", games_folders_info, "Where the Games tab finds games: the start folder, plus folders added from the Folders tab (Z, \"Add this folder to Games\")."),
    INFO("Start Folder", default_folder, "The folder the menu opens in. Change it from Options on the Files screen."),
    ACTION("Forget Added Folders", forget_games_folders, "The Games tab goes back to showing the start folder only, and no folder stays marked as hacks. No files are touched."),
    INFO("Hidden Games", hidden_info, "Games hidden from the cover tabs with Z, \"Hide this game\". They stay on the card and in the Folders tab, where Z, \"Unhide this game\", brings one back."),
    ACTION("Unhide All Games", show_hidden_games, "Bring every hidden game back to the cover tabs. No files are touched."),
    ACTION("Find Missing Metadata", make_art_report, "Check every game on the Games tab for its cover, the back of its box, top label, cartridge picture, description, release date and developer. Games lacking any are listed in menu/missingart.txt on the SD card, for the Companion's Missing Metadata tab. Stay on this page until it is done."),
    INFO("Missing Metadata", art_report_info, "How many games lack something, from the last check. The Companion's Missing Metadata tab shows which, and fills them in."),
};

static const item_t play_items[] = {
    FEATURE("Back Up Saves", FEATURE_SAVE_BACKUPS, "When you start a game, keep its save from before, by date. Up to five per game, never the same one twice. A game's Options has \"Restore save\" to go back to one."),
    FEATURE("Count Plays", FEATURE_PLAY_STATS, "Keep count of how often each game is started and when it was last played, shown on its info screen."),
    FEATURE("Remember Position", FEATURE_REMEMBER_SELECTION, "Going back into a folder returns to the game you had selected there, also after playing."),
};

static const item_t video_items[] = {
    CHOICE("Video Output", force_progressive_scan, false, "480i", "240p", NULL,
        "480i is sharper but can flicker. 240p is steady with softer text, and suits TVs that struggle with interlaced video. Restart the console to apply."),
    SWITCH("PAL60 Mode", pal60_enabled, false, apply_pal60,
        "PAL consoles only. The picture may go dark if your TV can't show it; to undo that, edit menu/config.ini on the SD card."),
    CHOICES("Character Set", OPTION_FONT, FONT_COUNT, font_choice_name, NULL,
        "Latin Only frees about 700 KB of memory but can't show Japanese names. Auto uses Latin Only without the Expansion Pak, Full with it. Restart the console to apply."),
};

static const item_t screenshot_items[] = {
    FEATURE("Screenshots", FEATURE_SCREENSHOTS, "Hold the Screenshot Button for a second to save the screen as a picture in menu/screenshots on the SD card. The menu pauses a moment while it saves."),
    CHOICES("Screenshot Button", OPTION_SHOT_BUTTON, SCREENSHOT_BUTTON_COUNT, screenshot_button_name, NULL,
        "Hold START: a tap of START then works when it is let go. Hold L + R: for controllers whose START switches them off, such as the 8BitDo mod kit; L and R then work when let go."),
    CHOICES("Screenshot Size", OPTION_SHOT_SIZE, 2, screenshot_size_name, NULL,
        "Full keeps the menu's whole 640 x 480 picture. Half is 320 x 240, about what a 240p TV shows, with much smaller files."),
    CHOICES("Screenshot Dither", OPTION_SHOT_DITHER, 2, screenshot_dither_name, NULL,
        "The console draws in 16-bit color with a fine dot pattern that the TV hides. Smooth evens it out in the picture; Exact keeps every pixel as drawn (larger files)."),
};

static const item_t clip_items[] = {
    ACTION_INFO("Record Clip", clip_arm, clip_status_info, "Press A, then leave Settings: after three ticks the menu records a clip of what you do, frame by frame, into menu/clips on the SD card. The console runs slowly while it saves; the clip plays at full speed. The Companion turns it into a GIF. A again before leaving: never mind."),
    CHOICES("Clip Length", OPTION_CLIP_LENGTH, CLIP_LENGTH_COUNT, clip_length_name, NULL,
        "How long a clip lasts. Recording takes much longer than this: each frame is saved to the card as it is drawn."),
    CHOICES("Clip Size", OPTION_CLIP_SIZE, 2, screenshot_size_name, NULL,
        "Full keeps the menu's whole 640 x 480 picture, Half is 320 x 240. Half records about four times faster and makes a smaller GIF."),
    CHOICES("Clip Frame Rate", OPTION_CLIP_FPS, CLIP_FPS_COUNT, clip_fps_name, NULL,
        "30 is every frame the menu draws, the smoothest. 15 halves the frames to save and the size of the GIF."),
    CHOICES("Clip Dither", OPTION_CLIP_DITHER, 2, screenshot_dither_name, NULL,
        "Smooth evens out the console's fine dot pattern, which also makes the GIF smaller; Exact keeps every pixel as drawn."),
};

static const item_t tool_items[] = {
    ACTION("Controller Pak Manager", open_controller_paks, "The notes (game saves) on a Controller Pak: back them up to the SD card, restore one or a whole dump, delete one, or format the Pak."),
    ACTION("Test Patterns", open_test_patterns, "Pictures for setting up your TV: a grid with borders showing how much of the picture's edge it cuts off, and color bars with grey steps for brightness and color. A or right shows the next one, B comes back here."),
    ACTION("Controller Test", open_pad_test, "Every button lights up on a drawn controller as you press it, and the stick's range is measured to show how worn it is. With a Rumble Pak, Z + A rumbles it. Hold B for two seconds to come back here."),
    ACTION("Clock", open_clock, "Set the date and time of the cart's clock, used for the clock on the tab bar, dated save backups and time played."),
    INFO("Memory", memory_note_info, "Free memory now. When memory runs short the menu leaves extras out instead of stopping; any left out since power-on are listed here."),
    CHOICES("Performance Overlay", OPTION_PERF_OVERLAY, 2, debug_stats_overlay_name, NULL,
        "For testing: a strip at the bottom of the screen shows the frame time (33.4 ms is on time), how long the graphics chip took, and free memory."),
    FEATURE("Remember Settings Page", FEATURE_REMEMBER_SETTINGS, "Reopen Settings on the page and row you last used, until the console is switched off."),
    ACTION("Reset Settings", ask_reset, "Put the stock settings back to how they were on a fresh install."),
};

static const item_t control_items[] = {
    FEATURE("Quick Launch", FEATURE_QUICK_LAUNCH, "Tap A to start a game. Hold A to see its info instead."),
    FEATURE("Hold A To Launch", FEATURE_HOLD_LAUNCH, "Tap A to see a game's info. Hold A to start it."),
    FEATURE("Up/Down Also Scroll", FEATURE_UPDOWN_SCROLL, "Up and down move through the covers too. When this is off, up and down turn the game box over to show its back."),
    SWITCH("Wrap File List", wrap_file_list_scrolling, false, NULL, "Going past the last item jumps back to the first. On the cover rows the last game then sits to the left of the first."),
    CHOICES("Name Entry", OPTION_NAME_ENTRY, NAME_ENTRY_COUNT, name_keyboard_entry_name, NULL,
        "How names are typed (collections, profiles, themes, renaming files). Keyboard is an on-screen keyboard: A types, B erases, R is Shift, C-down a space, START or OK keeps the name. Dial spells it round a circle with the stick."),
};

static const item_t sound_items[] = {
    SWITCH("Sound Effects", soundfx_enabled, false, apply_soundfx, "Play sounds when moving around the menu."),
    SWITCH("Background Music", bgm_enabled, false, apply_bgm, "Play music in the menu."),
    SLIDER("Music Volume", OPTION_MUSIC_VOLUME, apply_volume, "How loud the menu's music plays. 5 is the usual level, 10 twice as loud. Left and right change it."),
    SLIDER("Sound Volume", OPTION_SOUND_VOLUME, apply_volume, "How loud the button sounds and the intro tune play. 5 is the usual level, 10 twice as loud. Left and right change it."),
};

static const item_t profile_items[] = {
    ACTION_INFO("Playing As", next_profile, playing_info, "Who is playing. Each profile has its own game saves: a new profile starts every game fresh. A switches to the next profile."),
    ACTION_INFO("New Profile", new_profile, new_profile_info, "Add a player (up to four), named with the letter dial. The new profile is switched to."),
    ACTION("Rename Profile", rename_profile, "Give the profile that is playing a new name. Its saves stay where they are."),
    ACTION_INFO("Profile Picture", choose_picture, picture_info, "The picture beside the player's name, in a ring of their color. A opens the pictures: those built into the menu, then any PNG in menu/avatars on the SD card. None shows the player's initial."),
    ACTION_INFO("Profile Color", next_color, color_info, "The color of the ring round the player's picture, and of their initial when they have none. A picks the next one."),
    CHOICES("Picture Shape", OPTION_AVATAR_SHAPE, AVATAR_SHAPE_COUNT, shape_name, shape_changed, "How every player's picture is cut: a circle or a rounded square."),
    ACTION("Delete Profile", delete_profile, "Take the profile that is playing off the list. Its saves stay on the SD card (in each saves folder, under its name), so nothing is lost. The first profile can't be deleted."),
};

static const item_t tab_items[] = {
    CHOICES("First Tab", OPTION_TAB1, TAB_CHOICE_VALUES, tabs_choice_name, NULL,
        "What the first place on the tab bar holds. The menu opens on this tab. L and R step through the tabs."),
    CHOICES("Second Tab", OPTION_TAB2, TAB_CHOICE_VALUES, tabs_choice_name, NULL,
        "What the second place on the tab bar holds. None leaves it out."),
    CHOICES("Third Tab", OPTION_TAB3, TAB_CHOICE_VALUES, tabs_choice_name, NULL,
        "What the third place on the tab bar holds. None leaves it out."),
    CHOICES("Fourth Tab", OPTION_TAB4, TAB_CHOICE_VALUES, tabs_choice_name, NULL,
        "What the fourth place on the tab bar holds. Choose Recent here to add the games you played last, Homebrew for homebrew games, Collections for series and multiplayer games, Retro for NES, SNES and Game Boy games, Hacks for ROM hacks, or any one collection (after Hacks) to give it a tab of its own."),
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

static const item_t information_items[] = {
    ACTION("Menu Information", open_credits, "The menu's version, the people and projects it is built on, and the music and sound credits."),
    ACTION("Flashcart Information", open_flashcart_info, "The SummerCart64's firmware version and what it supports."),
    ACTION("N64 Information", open_system_info, "The console: Expansion Pak or not, region, the controllers plugged in and whether a 64DD drive is attached."),
};

static const category_t look_pages[] = {
    { "Theme", theme_items, COUNT(theme_items) },
    { "Covers", cover_items, COUNT(cover_items) },
    { "3D Boxes", box_items, COUNT(box_items) },
    { "Badges", badge_items, COUNT(badge_items) },
    { "Game Info", game_info_items, COUNT(game_info_items) },
    { "Assistant", assistant_items, COUNT(assistant_items) },
    { "Start-up", startup_items, COUNT(startup_items) },
};

static const category_t games_pages[] = {
    { "Library", library_items, COUNT(library_items) },
    { "Game Names", name_items, COUNT(name_items) },
    { "Game Folders", folder_items, COUNT(folder_items) },
    { "Saves & Play", play_items, COUNT(play_items) },
    { "Tabs", tab_items, COUNT(tab_items) },
    { "Profiles", profile_items, COUNT(profile_items) },
    { "Files", file_items, COUNT(file_items) },
};

static const category_t console_pages[] = {
    { "Controls", control_items, COUNT(control_items) },
    { "Sound", sound_items, COUNT(sound_items) },
    { "TV & Video", video_items, COUNT(video_items) },
    { "Screenshots", screenshot_items, COUNT(screenshot_items) },
    { "Clips", clip_items, COUNT(clip_items) },
    { "Tools", tool_items, COUNT(tool_items) },
    { "About", information_items, COUNT(information_items) },
};

/* The headings along the top, switched with L and R; each holds its pages. */
static const group_t groups[] = {
    { "Look", look_pages, COUNT(look_pages) },
    { "Games", games_pages, COUNT(games_pages) },
    { "Console", console_pages, COUNT(console_pages) },
};

/* ---------- state ---------- */

static int group = 0;           /* the heading along the top: Look, Games, Console */
static int category = 0;        /* the page within that heading */
static int item = 0;
static bool in_items = false;   /* false: choosing a page, true: inside one */
static int first_row = 0;       /* first setting shown, when a category has more than fit */

static void open_sub_page (const category_t *page) {
    sub_from_item = item;
    sub_page = page;
    item = 0;
    first_row = 0;
}

/* The page shown: a row's own page while one is open, else the chosen one. */
static const category_t *current_page (void) {
    return sub_page ? sub_page : &groups[group].pages[category];
}

static bool *switch_value (menu_t *menu, const item_t *it) {
    return (bool *) ((char *) &menu->settings + it->offset);
}

/* A choice with more than four values opens a list to pick from instead of
   stepping through them one A press at a time. */
#define PICK_DIRECT_MAX     (4)
#define PICK_VALUES_MAX     (128)
#define PICK_ROWS_SHOWN     (9)
#define PICK_ROW_HEIGHT     (26)
#define PICK_WIDTH          (360)

static const item_t *picking = NULL;    /* the setting whose list is open */
static int pick_values[PICK_VALUES_MAX];    /* its values in use (those with a name) */
static int pick_count = 0;
static int pick_row = 0, pick_first = 0;

/* The values of a choice that are in use. */
static int choice_values (const item_t *it, int *out, int most) {
    int n = 0;
    for (int v = 0; v < it->choice_count && n < most; v++) {
        if (it->choice_name(v)[0]) {
            out[n++] = v;
        }
    }
    return n;
}

static bool pick_open (const item_t *it) {
    pick_count = choice_values(it, pick_values, PICK_VALUES_MAX);
    if (pick_count <= PICK_DIRECT_MAX) {
        return false;
    }
    picking = it;
    pick_row = 0;
    for (int i = 0; i < pick_count; i++) {
        if (pick_values[i] == options_get(it->option)) {
            pick_row = i;
        }
    }
    pick_first = (pick_row >= PICK_ROWS_SHOWN) ? pick_row - (PICK_ROWS_SHOWN / 2) : 0;
    if (pick_first > pick_count - PICK_ROWS_SHOWN) {
        pick_first = pick_count - PICK_ROWS_SHOWN;
    }
    if (pick_first < 0) {
        pick_first = 0;     /* a list shorter than the window */
    }
    return true;
}

static void pick_process (menu_t *menu) {
    int step = menu->actions.go_fast ? PICK_ROWS_SHOWN : 1;
    if (menu->actions.go_up && pick_row > 0) {
        pick_row = (pick_row - step < 0) ? 0 : pick_row - step;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && pick_row < pick_count - 1) {
        pick_row = (pick_row + step >= pick_count) ? pick_count - 1 : pick_row + step;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        options_change(picking->option, pick_values[pick_row]);
        if (picking->changed) {
            picking->changed(menu);
        }
        unsaved = true;
        changed_at = get_ticks_ms();
        picking = NULL;
        sound_play_effect(SFX_SETTING);
        return;
    } else if (menu->actions.back) {
        picking = NULL;
        sound_play_effect(SFX_EXIT);
        return;
    }
    if (pick_row < pick_first) {
        pick_first = pick_row;
    } else if (pick_row >= pick_first + PICK_ROWS_SHOWN) {
        pick_first = pick_row - PICK_ROWS_SHOWN + 1;
    }
}

static void pick_draw (void) {
    if (!picking) {
        return;
    }
    int shown = (pick_count < PICK_ROWS_SHOWN) ? pick_count : PICK_ROWS_SHOWN;
    int height = 40 + (shown * PICK_ROW_HEIGHT) + 8;
    ui_components_dialog_draw(PICK_WIDTH, height);
    int x0 = DISPLAY_CENTER_X - (PICK_WIDTH / 2);
    int y0 = DISPLAY_CENTER_Y - (height / 2);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = PICK_WIDTH, .height = 26, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES, .style_id = STL_GRAY }, FNT_DEFAULT, x0, y0 + 6, "%s", picking->label);
    int current = options_get(picking->option);
    static animations_glide_t pick_glide;
    int bar_y = animations_glide(&pick_glide, y0 + 34 + ((pick_row - pick_first) * PICK_ROW_HEIGHT));   /* Showy: the bar glides between rows */
    ui_components_box_draw(x0 + 8, bar_y, x0 + PICK_WIDTH - 8, bar_y + PICK_ROW_HEIGHT, CONTEXT_MENU_HIGHLIGHT_COLOR);
    for (int i = 0; i < shown; i++) {
        int r = pick_first + i;
        int y = y0 + 34 + (i * PICK_ROW_HEIGHT);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = PICK_WIDTH - 72, .height = PICK_ROW_HEIGHT, .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES, .style_id = (pick_values[r] == current) ? STL_GREEN : STL_DEFAULT },
            FNT_DEFAULT, x0 + 24, y, "%s", picking->choice_name(pick_values[r]));
    }
    if (pick_first > 0) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = PICK_ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT,
            x0 + PICK_WIDTH - 36, y0 + 34, "▲");
    }
    if (pick_first + shown < pick_count) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = 20, .height = PICK_ROW_HEIGHT, .valign = VALIGN_CENTER }, FNT_DEFAULT,
            x0 + PICK_WIDTH - 36, y0 + 34 + ((shown - 1) * PICK_ROW_HEIGHT), "▼");
    }
    animations_dialog_end();
}

/* Rows with a value that left and right step through (A steps forward). */
static bool has_value (const item_t *it) {
    return it->type == ITEM_SWITCH || it->type == ITEM_FEATURE || it->type == ITEM_CHOICE;
}

/* dir: 1 the next value (A, right), -1 the one before (left). */
static void change_item (menu_t *menu, const item_t *it, int dir) {
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
            /* Default -> On -> Off -> Default (left goes the other way) */
            int value = features_user_get(it->feature);
            int next = (dir > 0) ? ((value == FEATURE_UNSET) ? 1 : (value == 1) ? 0 : FEATURE_UNSET)
                                 : ((value == FEATURE_UNSET) ? 0 : (value == 0) ? 1 : FEATURE_UNSET);
            features_user_change(it->feature, next);
            if (it->feature == FEATURE_REVIEW_SCORES) {
                resort_browser(menu);   /* Sort By's Review Score comes and goes with it */
            }
            break;
        }
        case ITEM_CHOICE: {
            int next = options_get(it->option);
            for (int tries = 0; tries < it->choice_count; tries++) {
                next = (next + dir + it->choice_count) % it->choice_count;
                if (it->choice_name(next)[0]) {
                    break;      /* a value with no name is not in use (a tab for a collection there isn't) */
                }
            }
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
        case ITEM_SLIDER:
            break;
    }

    unsaved = true;
    changed_at = get_ticks_ms();
}

static void process (menu_t *menu) {
    if (picking) {
        pick_process(menu);     /* a setting's list of values is open */
        return;
    }
    if (notice) {
        if (menu->actions.enter || menu->actions.back || menu->actions.options || menu->actions.settings || menu->actions.lz_context
            || menu->actions.go_up || menu->actions.go_down || menu->actions.go_left || menu->actions.go_right) {
            notice = NULL;  /* any button */
            sound_play_effect(SFX_EXIT);
        }
        return;
    }
    if (letter_wheel_naming()) {
        letter_wheel_name_process(menu);    /* naming a profile or a theme */
        return;
    }
    if (theme_editor_active()) {
        theme_editor_process(menu);
        return;
    }
    if (tv_patterns_active()) {
        tv_patterns_process(menu);     /* Console > Tools > Test Patterns */
        return;
    }
    if (pad_test_active()) {
        pad_test_process(menu);        /* Console > Tools > Controller Test */
        return;
    }
    if (assistant_ask_active()) {
        assistant_ask_process(menu);   /* Look > Assistant > Ask a Question */
        return;
    }
    if (avatar_pick_active()) {
        avatar_pick_process(menu);
        return;
    }
    if (confirm_delete >= 0) {
        if (menu->actions.enter) {
            profiles_delete(confirm_delete);
            profile_pick_refresh(menu);     /* deleting the player in use goes back to the first */
            confirm_delete = -1;
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            confirm_delete = -1;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }
    if (confirm_reset) {
        if (menu->actions.enter) {
            confirm_reset = false;
            settings_dirty = false; // don't write the old values back over the reset
            settings_reset_to_defaults();
            notice = "Reboot N64 to take effect!";
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            confirm_reset = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    /* L and R step through the headings, going round, back to their first page. */
    joypad_buttons_t pressed = screenshot_pressed();
    if (pressed.l || pressed.r) {
        group = (group + (pressed.r ? 1 : COUNT(groups) - 1)) % COUNT(groups);
        sub_page = NULL;
        category = 0;
        item = 0;
        in_items = false;
        sound_play_effect(SFX_CURSOR);
        return;
    }

    const category_t *cat = current_page();

    if (!in_items) {
        if (menu->actions.go_up && category > 0) {
            category--;
            item = 0;
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.go_down && category < groups[group].count - 1) {
            category++;
            item = 0;
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.enter || menu->actions.go_right) {
            in_items = true;
            item = 0;
            sound_play_effect(SFX_ENTER);
        } else if (menu->actions.back) {
            menu->next_mode = games_ui_origin(); // back to the tab Settings was opened from
            if (animations_showy()) {
                tabs_slide_begin(menu, -1);     // Showy: the tab's covers come back in from the side
            }
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
    } else if (cat->items[item].type == ITEM_SLIDER && (menu->actions.go_left || menu->actions.go_right)) {
        const item_t *it = &cat->items[item];
        int v = options_get(it->option) + (menu->actions.go_right ? 1 : -1);
        if (v >= 0 && v <= VOLUME_STEPS) {
            options_change(it->option, v);
            it->changed(menu);
            unsaved = true;
            changed_at = get_ticks_ms();
            if (it->option == OPTION_SOUND_VOLUME) {
                sound_play_effect(SFX_SETTING);     /* heard at the new level; the music slider stays quiet so the music can be heard */
            }
        }
    } else if (menu->actions.enter) {
        if (cat->items[item].type == ITEM_SLIDER) {
            return;
        }
        if (cat->items[item].type == ITEM_CHOICE && pick_open(&cat->items[item])) {
            sound_play_effect(SFX_ENTER);   /* more than four values: a list to pick from */
            return;
        }
        change_item(menu, &cat->items[item], 1);
        sound_play_effect(SFX_SETTING);
    } else if (has_value(&cat->items[item]) && (menu->actions.go_left || menu->actions.go_right)) {
        change_item(menu, &cat->items[item], menu->actions.go_right ? 1 : -1);   /* the arrows beside the value */
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.back || menu->actions.go_left) {
        if (sub_page) {
            sub_page = NULL;            /* back to the row it was opened from */
            item = sub_from_item;
        } else {
            in_items = false;
        }
        sound_play_effect(SFX_EXIT);
    }
}

/* ---------- drawing ---------- */

#define ROW_HEIGHT      (26)
#define TITLE_Y         (VISIBLE_AREA_Y0 + 10)
#define ROWS_Y          (TITLE_Y + 32)
#define LEFT_X          (VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL)
#define LEFT_WIDTH      (148)
#define DIVIDER_X       (LEFT_X + LEFT_WIDTH + 8)
#define ITEMS_X         (DIVIDER_X + 12)
#define ITEMS_WIDTH     (VISIBLE_AREA_X1 - TEXT_MARGIN_HORIZONTAL - ITEMS_X)
#define VISIBLE_ROWS    (8)     /* settings shown at once; longer lists scroll */
#define PANEL_PAD       (4)     /* the panels reach this far round their rows */
#define HELP_Y          (ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT) + PANEL_PAD + 6)  /* top of the help panel */
#define HELP_GAP        (4)     /* between the help panel and the hint band */
#define HELP_PAD        (8)     /* inside the help panel */
#define HELP_HEADING    (22)
#define ROW_PADDING     (6)
#define INFO_NAME_WIDTH (176)   /* an information row's name ("Missing Metadata"); its value gets the rest */
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
        games_ui_round_fill(x, y, x + width, y + ROW_HEIGHT, FILE_LIST_HIGHLIGHT_COLOR);
    } else {
        draw_fill(x + 4, y + ROW_HEIGHT - 2, x + width - 4, y + ROW_HEIGHT, theme_get()->accent);
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

/* Each page's icon (rom:/settings_icons.sprite: 18 x 18 pictures side by
   side, made by make_icons.py), tinted in the text color. */
#define PAGE_ICON       (18)

static void draw_page_icon (int index, int x, int y) {
    static sprite_t *icons = NULL;
    if (!icons) {
        icons = sprite_load("rom:/settings_icons.sprite");     /* 3.4 KB, kept */
    }
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(theme_get()->text);
        rdpq_sprite_blit(icons, x, y, &(rdpq_blitparms_t) { .s0 = index * PAGE_ICON, .width = PAGE_ICON });
    rdpq_mode_pop();
}

#define FAIRY_PREVIEW   (42)    /* the fairy on the Fairy Look page */
#define GROUP_TAB_WIDTH (100)
#define GROUP_TAB_GAP   (6)
#define GROUP_TAB_Y     (TITLE_Y - 2)
#define PLAYER_PICTURE  (24)    /* the player's picture at the top right, with 2+ profiles */

/* The headings as tabs, like the tab bar on the cover rows: the one in use
   an accent block with white letters (Showy: the block glides). */
static void draw_group_tabs (void) {
    const theme_t *t = theme_get();
    for (int i = 0; i < COUNT(groups); i++) {
        int x = LEFT_X + i * (GROUP_TAB_WIDTH + GROUP_TAB_GAP);
        games_ui_round_fill(x, GROUP_TAB_Y, x + GROUP_TAB_WIDTH, GROUP_TAB_Y + ROW_HEIGHT, t->tab_inactive);
    }
    static animations_glide_t block_glide;
    int block_x = animations_glide(&block_glide, LEFT_X + group * (GROUP_TAB_WIDTH + GROUP_TAB_GAP));
    games_ui_round_fill(block_x, GROUP_TAB_Y, block_x + GROUP_TAB_WIDTH, GROUP_TAB_Y + ROW_HEIGHT, t->accent);
    for (int i = 0; i < COUNT(groups); i++) {
        int x = LEFT_X + i * (GROUP_TAB_WIDTH + GROUP_TAB_GAP);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = GROUP_TAB_WIDTH, .height = ROW_HEIGHT, .align = ALIGN_CENTER,
            .valign = VALIGN_CENTER, .style_id = (i == group) ? STL_WHITE : STL_GRAY }, FNT_DEFAULT, x, GROUP_TAB_Y, "%s", groups[i].label);
    }
}

static const char *on_off (bool on) {
    return on ? "On" : "Off";
}

static const char *switch_text (const item_t *it, bool on) {
    const char *text = on ? it->on_text : it->off_text;
    return text ? text : on_off(on);
}

#define SLIDER_WIDTH    (150)
#define SLIDER_NUMBER   (36)    /* room for "10" after the bar */

/* A bar in ten steps, filled up to the value, and the value after it. */
static void draw_slider (const item_t *it, int y) {
    int v = options_get(it->option);
    char number[8];
    snprintf(number, sizeof(number), "%d", v);
    draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
    draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_RIGHT, STL_DEFAULT, number);
    int x1 = ITEMS_X + ITEMS_WIDTH - ROW_PADDING - SLIDER_NUMBER;
    int x0 = x1 - SLIDER_WIDTH;
    int step = SLIDER_WIDTH / VOLUME_STEPS;
    int y0 = y + (ROW_HEIGHT / 2) - 5;
    for (int i = 0; i < VOLUME_STEPS; i++) {
        int sx = x0 + i * step;
        draw_fill(sx, y0, sx + step - 4, y0 + 10, (i < v) ? theme_get()->accent : theme_get()->tab_inactive);
    }
}

#define PILL_WIDTH      (48)
#define PILL_HEIGHT     (20)
#define DOT_SIZE        (6)
#define DOT_STEP        (10)
#define DOTS_MOST       (6)     /* more values than this get no dots (themes, tabs, sort orders) */

/* On and Off as a small rounded pill at the row's right: green On, grey Off. */
static void draw_pill (int y, bool on) {
    int x1 = ITEMS_X + ITEMS_WIDTH - ROW_PADDING;
    int y0 = y + (ROW_HEIGHT - PILL_HEIGHT) / 2;
    games_ui_round_fill(x1 - PILL_WIDTH, y0, x1, y0 + PILL_HEIGHT, on ? RGBA32(0x14, 0x94, 0x34, 0xFF) : theme_get()->tab_inactive);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = PILL_WIDTH, .height = PILL_HEIGHT, .align = ALIGN_CENTER, .valign = VALIGN_CENTER,
        .style_id = on ? STL_WHITE : STL_GRAY }, FNT_DEFAULT, x1 - PILL_WIDTH, y0, "%s", on_off(on));
}

/* A dot per value, the one in use in the accent color. @return The room they took. */
static int draw_dots (int y, int count, int current) {
    if (count < 2 || count > DOTS_MOST) {
        return 0;
    }
    int x1 = ITEMS_X + ITEMS_WIDTH - ROW_PADDING;
    int x = x1 - (count * DOT_STEP) + (DOT_STEP - DOT_SIZE);
    int y0 = y + (ROW_HEIGHT - DOT_SIZE) / 2;
    for (int i = 0; i < count; i++, x += DOT_STEP) {
        draw_fill(x, y0, x + DOT_SIZE, y0 + DOT_SIZE, (i == current) ? theme_get()->accent : theme_get()->tab_inactive);
    }
    return (count * DOT_STEP) + 4;
}

#define CHANGED_WIDTH   (4)     /* the accent bar beside a changed row */
#define CHANGED_HEIGHT  (14)

/* Whether the player has changed a row from how it comes: a switch from its
   stock value, a feature from Default, a choice or slider from its default. */
static bool is_changed (menu_t *menu, const item_t *it) {
    switch (it->type) {
        case ITEM_SWITCH:   return *switch_value(menu, it) != it->stock_default;
        case ITEM_FEATURE:  return features_user_get(it->feature) != FEATURE_UNSET;
        case ITEM_CHOICE:
        case ITEM_SLIDER:   return options_get(it->option) != options_default(it->option);
        default:            return false;
    }
}

static void draw_item (menu_t *menu, const item_t *it, int y, bool selected) {
    if (is_changed(menu, it)) {
        /* Changed: a short accent bar in the panel's margin beside the row. */
        int y0 = y + (ROW_HEIGHT - CHANGED_HEIGHT) / 2;
        draw_fill(ITEMS_X - PANEL_PAD, y0, ITEMS_X - PANEL_PAD + CHANGED_WIDTH, y0 + CHANGED_HEIGHT, theme_get()->accent);
    }

    const char *value = "";
    menu_font_style_t style = STL_DEFAULT;

    int dots = 0;       /* room the value's dots take at the right */
    bool arrows = false;

    switch (it->type) {
        case ITEM_SWITCH:
            if (!it->on_text && !it->off_text) {
                draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
                draw_pill(y, *switch_value(menu, it));
                return;
            }
            value = switch_text(it, *switch_value(menu, it));     /* two named values (480i / 240p) */
            dots = draw_dots(y, 2, *switch_value(menu, it) ? 1 : 0);
            arrows = selected;
            break;
        case ITEM_FEATURE: {
            int user = features_user_get(it->feature);
            if (user == FEATURE_UNSET) {
                /* Not chosen by the player: show what it comes out as, dimmed.
                   (Usually the theme's value; a launch mode shows Off while
                   the other launch mode is on.) */
                value = features_enabled(it->feature) ? "Default (On)" : "Default (Off)";
                style = STL_GRAY;
                arrows = selected;
            } else {
                draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
                draw_pill(y, user != 0);
                return;
            }
            break;
        }
        case ITEM_CHOICE: {
            int current = options_get(it->option);
            value = it->choice_name(current);
            if (!value[0]) {
                value = it->choice_name(0);     /* a value not in use now (Review Score with Review Scores off) works as the first */
                current = 0;
            }
            int values[DOTS_MOST + 1];
            int count = choice_values(it, values, DOTS_MOST + 1);
            int place = 0;
            for (int i = 0; i < count && i <= DOTS_MOST; i++) {
                if (values[i] == current) {
                    place = i;
                }
            }
            dots = draw_dots(y, count, place);
            arrows = selected;
            break;
        }
        case ITEM_ACTION:
            if (it->info) {
                value = it->info(menu);     /* an action that also says how it stands (Record Clip) */
                style = STL_GRAY;
            }
            break;
        case ITEM_INFO:
            value = it->info(menu);
            style = STL_GRAY;
            break;
        case ITEM_SLIDER:
            draw_slider(it, y);
            return;
    }

    if (it->type == ITEM_INFO) {
        /* Long values (folder paths) get most of the row. */
        draw_text(ITEMS_X, y, INFO_NAME_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
        draw_text(ITEMS_X + INFO_NAME_WIDTH, y, ITEMS_WIDTH - INFO_NAME_WIDTH, ALIGN_RIGHT, style, value);
    } else if (arrows) {
        /* The selected row: arrows say left and right change it. */
        char shown[96];
        snprintf(shown, sizeof(shown), "◀ %s ▶", value);
        draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
        draw_text(ITEMS_X, y, ITEMS_WIDTH - dots, ALIGN_RIGHT, style, shown);
    } else {
        draw_text(ITEMS_X, y, ITEMS_WIDTH, ALIGN_LEFT, STL_DEFAULT, it->label);
        draw_text(ITEMS_X, y, ITEMS_WIDTH - dots, ALIGN_RIGHT, style, value);
    }
}

/* The button hints: one row with the cover rows' icons, in the lower of
   their two places, on the same dark band (only round that row, so the
   help above keeps its room). */
static int hints_top (void) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    return y0 + ((y1 - y0) / 2);
}

#define LEGEND_TEXT     (66)    /* "Changed" in the body font */

static void draw_hints (const char *a_action, bool slider, bool any_changed) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    const theme_t *t = theme_get();
    games_ui_round_fill(x0, hints_top(), x1, y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
    int x = GAMES_UI_HINTS_X;
    if (slider) {
        x = games_ui_hint_draw(x, 1, "◀▶", "Change");
    } else if (a_action) {
        x = games_ui_hint_draw(x, 1, "A", a_action);
    }
    x = games_ui_hint_draw(x, 1, "B", "Back");
    games_ui_hint_draw(x, 1, "L R", "Change Tab");

    /* What the accent bar beside a row means, against the right edge, while
       the page shown has one. Lined up with the hints' row. */
    if (any_changed) {
        int row = (y1 - y0 - 6) / 2;            /* a hint row's height (games_ui.c) */
        int row_y = y1 - 1 - row;               /* the lower row's top */
        int height = row - 4;                   /* a hint's own height */
        int lx = GAMES_UI_CONTENT_X1 - (CHANGED_WIDTH + 6 + LEGEND_TEXT);
        int bar_y = row_y + (height - CHANGED_HEIGHT) / 2;
        draw_fill(lx, bar_y, lx + CHANGED_WIDTH, bar_y + CHANGED_HEIGHT, theme_get()->accent);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = LEGEND_TEXT + 4, .height = height, .valign = VALIGN_CENTER,
            .style_id = STL_GRAY }, FNT_DEFAULT, lx + CHANGED_WIDTH + 6, row_y, "Changed");
    }
}

/* The panel along the bottom: a heading, then the help in grey, the full
   width of both columns. */
#define HELP_BADGE_WIDTH (178)

static void draw_help_box (const char *heading, const char *help, const char *note, const char *badge) {
    int x0 = LEFT_X - PANEL_PAD, x1 = ITEMS_X + ITEMS_WIDTH + PANEL_PAD;
    int y1 = hints_top() - HELP_GAP;
    games_ui_round_fill(x0, HELP_Y, x1, y1, theme_get()->panel);
    rdpq_text_printf(&(rdpq_textparms_t) { .width = x1 - x0 - (HELP_PAD * 2), .height = HELP_HEADING, .valign = VALIGN_CENTER,
        .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT }, FNT_DEFAULT, x0 + HELP_PAD, HELP_Y + 2, "%s", heading);
    if (badge) {
        /* A warning about the setting, in orange at the heading's right. */
        int bx1 = x1 - HELP_PAD, by0 = HELP_Y + 2 + (HELP_HEADING - PILL_HEIGHT) / 2;
        games_ui_round_fill(bx1 - HELP_BADGE_WIDTH, by0, bx1, by0 + PILL_HEIGHT, RGBA32(0xD8, 0x70, 0x10, 0xFF));
        rdpq_text_printf(&(rdpq_textparms_t) { .width = HELP_BADGE_WIDTH, .height = PILL_HEIGHT, .align = ALIGN_CENTER,
            .valign = VALIGN_CENTER, .style_id = STL_WHITE }, FNT_DEFAULT, bx1 - HELP_BADGE_WIDTH, by0, "%s", badge);
    }
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = x1 - x0 - (HELP_PAD * 2),
        .height = y1 - (HELP_Y + 2 + HELP_HEADING) - 2,
        .wrap = WRAP_WORD,
        .style_id = STL_GRAY,
    }, FNT_DEFAULT, x0 + HELP_PAD, HELP_Y + 2 + HELP_HEADING, "%s%s", help, note);
}

/* On the page list: the page's name and what is on it. */
static void draw_page_help (const category_t *cat) {
    static char rows[512];
    int n = 0;
    rows[0] = '\0';
    for (int i = 0; i < cat->count && n < (int) sizeof(rows) - 1; i++) {
        n += snprintf(rows + n, sizeof(rows) - n, "%s%s", (i > 0) ? ", " : "", cat->items[i].label);
    }
    draw_help_box(cat->label, rows, "", NULL);
}

static void draw_help (menu_t *menu, const item_t *it) {
    const char *note = "";
    const char *badge = NULL;
    char stock[64];
    bool console = (it->type == ITEM_SWITCH) || (it->type == ITEM_CHOICE && (it->option == OPTION_FONT || it->option == OPTION_AVATAR_SHAPE))
        || (it->type == ITEM_INFO && it->info == default_folder);     /* config.ini and Character Set: not per profile */

    if (it->type == ITEM_SWITCH) {
        snprintf(stock, sizeof(stock), "\nStock value: %s%s", switch_text(it, it->stock_default),
            (profiles_count() >= 2) ? ". Same for every player." : "");
        note = stock;
    } else if (console && profiles_count() >= 2) {
        note = "\nSame for every player.";
    } else if (it->type == ITEM_FEATURE) {
        if (!features_available(it->feature)) {
            badge = "Needs Expansion Pak";
        } else {
            note = "\nDefault follows the theme. A or left / right: Default, On, Off.";
        }
    } else if (it->type == ITEM_INFO && it->info == memory_note_info) {
        note = memory_note_details();
    }

    draw_help_box(it->label, it->help, note, badge);
}

#define PAGE_SLIDE_MS   (180)
#define PAGE_SLIDE_PX   (24)
#define ROWS_RISE_MS    (240)
#define ROWS_RISE_PX    (40)
static bool rows_rise_pending = false;

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach(d, NULL);

    if (tv_patterns_active()) {
        tv_patterns_draw();     // the whole screen: no background under it
        debug_stats_overlay_draw();
        rdpq_detach_show();
        return;
    }

    ui_components_background_draw();

    ui_components_layout_draw();

    if (letter_wheel_keyboard_showing()) {
        letter_wheel_draw();    // naming a profile or a theme: the keyboard hides the page, so it isn't drawn
        debug_stats_overlay_draw();
        rdpq_detach_show();
        return;
    }

    if (pad_test_active()) {
        pad_test_draw();
        debug_stats_overlay_draw();
        rdpq_detach_show();
        return;
    }

    if (theme_editor_active()) {
        theme_editor_draw();
        letter_wheel_draw();    // naming the theme
        debug_stats_overlay_draw();
        rdpq_detach_show();
        return;
    }

    if (assistant_ask_active()) {
        assistant_ask_draw();
        animations_draw_late(menu->mode);
        debug_stats_overlay_draw();
        rdpq_detach_show();
        return;
    }

    if (avatar_pick_active()) {
        avatar_pick_draw();
        debug_stats_overlay_draw();
        rdpq_detach_show();
        return;
    }

    const category_t *cat = current_page();

    draw_group_tabs();
    if (profiles_count() >= 2) {
        /* Whose settings these are: the player's picture in a ring of their
           color at the top right, their name before it (as under the tabs). */
        int p = profiles_current();
        int x1 = ITEMS_X + ITEMS_WIDTH + PANEL_PAD;
        int name_x0 = LEFT_X + (3 * (GROUP_TAB_WIDTH + GROUP_TAB_GAP)) + 8;
        avatars_draw(profiles_picture(p), x1 - PLAYER_PICTURE, GROUP_TAB_Y + (ROW_HEIGHT - PLAYER_PICTURE) / 2, PLAYER_PICTURE,
                     profiles_color(p), profiles_name(p));
        rdpq_text_printf(&(rdpq_textparms_t) { .width = x1 - PLAYER_PICTURE - 8 - name_x0, .height = ROW_HEIGHT, .align = ALIGN_RIGHT,
            .valign = VALIGN_CENTER, .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT }, FNT_DEFAULT, name_x0, GROUP_TAB_Y, "%s", profiles_name(p));
    } else {
        draw_text(ITEMS_X, TITLE_Y, ITEMS_WIDTH, ALIGN_RIGHT, STL_GRAY, cat->label);
    }

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

    // Showy: on opening, both columns' rows rise into place, kept to the
    // rows' area.
    static uint64_t rise_since = 0;
    if (rows_rise_pending) {
        rows_rise_pending = false;
        rise_since = get_ticks_ms();    /* from the first frame drawn, not from opening: that can take a moment */
    }
    float rise_t = animations_progress(rise_since, ROWS_RISE_MS);
    int rise = (rise_t < 1.0f) ? (int) ((1.0f - animations_ease_out(rise_t)) * ROWS_RISE_PX) & ~1 : 0;
    if (rise != 0) {
        rdpq_set_scissor(LEFT_X, ROWS_Y, display_get_width(), ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT));
    }

    /* Rounded panels behind the page list and the rows, in the theme's panel color. */
    int panel_y0 = ROWS_Y - PANEL_PAD, panel_y1 = ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT) + PANEL_PAD;
    if (rise != 0) {
        rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
    }
    games_ui_round_fill(LEFT_X - PANEL_PAD, panel_y0, LEFT_X + LEFT_WIDTH + PANEL_PAD, panel_y1, theme_get()->panel);
    games_ui_round_fill(ITEMS_X - PANEL_PAD, panel_y0, ITEMS_X + ITEMS_WIDTH + PANEL_PAD, panel_y1, theme_get()->panel);
    if (rise != 0) {
        rdpq_set_scissor(LEFT_X, ROWS_Y, display_get_width(), ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT));
    }

    static animations_glide_t category_glide, item_glide;
    draw_selection(LEFT_X, animations_glide(&category_glide, ROWS_Y + (category * ROW_HEIGHT)) + rise, LEFT_WIDTH, !in_items);   /* Showy: glides */
    int first_icon = 0;     /* the icons are in page order, heading by heading */
    for (int g = 0; g < group; g++) {
        first_icon += groups[g].count;
    }
    for (int i = 0; i < groups[group].count; i++) {
        int y = ROWS_Y + (i * ROW_HEIGHT) + rise;
        /* Tight round the icon, and no padding at the name's end: "Game Folders" just fits. */
        int name_x = LEFT_X + 4 + PAGE_ICON + 4;
        draw_page_icon(first_icon + i, LEFT_X + 4, y + (ROW_HEIGHT - PAGE_ICON) / 2);
        rdpq_text_printf(&(rdpq_textparms_t) { .width = LEFT_X + LEFT_WIDTH - name_x, .height = ROW_HEIGHT, .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT }, FNT_DEFAULT, name_x, y, "%s", groups[group].pages[i].label);
    }

    // A page with more rows than fit gets a bar showing how far down you are.
    if (cat->count > VISIBLE_ROWS) {
        int track = VISIBLE_ROWS * ROW_HEIGHT;
        int thumb = (track * VISIBLE_ROWS) / cat->count;
        int thumb_y = ROWS_Y + ((track - thumb) * first_row) / (cat->count - VISIBLE_ROWS);
        draw_fill(SCROLLBAR_X, ROWS_Y, SCROLLBAR_X + SCROLLBAR_WIDTH, ROWS_Y + track, theme_get()->tab_inactive);
        draw_fill(SCROLLBAR_X, thumb_y, SCROLLBAR_X + SCROLLBAR_WIDTH, thumb_y + thumb, theme_get()->accent);
    }

    // Showy: another category's rows slide in from the way the selection
    // went, kept to the rows' area.
    static int shown_category = -1;
    static uint64_t category_since = 0;
    static int category_dir = 1;
    int page_key = (group * 32) + category + (sub_page ? 16 : 0);     /* pages are numbered heading by heading; a row's page after its own */
    if (page_key != shown_category) {
        if (shown_category >= 0) {
            category_dir = (page_key > shown_category) ? 1 : -1;
            category_since = get_ticks_ms();
        }
        shown_category = page_key;
    }
    float page_t = animations_progress(category_since, PAGE_SLIDE_MS);
    int page_shift = (page_t < 1.0f) ? ((int) ((1.0f - animations_ease_out(page_t)) * PAGE_SLIDE_PX) & ~1) * category_dir : 0;
    if (page_shift != 0) {
        rdpq_set_scissor(ITEMS_X - PANEL_PAD, ROWS_Y, display_get_width(), ROWS_Y + (VISIBLE_ROWS * ROW_HEIGHT));   /* the changed marks slide too */
    }
    if (in_items) {
        draw_selection(ITEMS_X, animations_glide(&item_glide, ROWS_Y + ((item - first_row) * ROW_HEIGHT)) + rise, ITEMS_WIDTH, true);
    }
    for (int i = first_row; i < last_row; i++) {
        int y = ROWS_Y + ((i - first_row) * ROW_HEIGHT) + page_shift + rise;
        draw_item(menu, &cat->items[i], y, in_items && i == item);
    }
    if (page_shift != 0 || rise != 0) {
        rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
    }

    if (in_items) {
        draw_help(menu, &cat->items[item]);
    } else {
        draw_page_help(cat);
    }
    if (cat->items == fairy_items) {
        /* The fairy as chosen, perched on the help panel's top right corner. */
        assistant_portrait_draw(ITEMS_X + ITEMS_WIDTH + PANEL_PAD - (FAIRY_PREVIEW / 2) - 6, HELP_Y - 4, FAIRY_PREVIEW);
    } else {
        assistant_portrait_free();
    }

    bool any_changed = false;
    for (int i = 0; i < cat->count; i++) {
        any_changed |= is_changed(menu, &cat->items[i]);
    }
    if (in_items) {
        const item_t *it = &cat->items[item];
        draw_hints((it->type == ITEM_ACTION) ? "Select" : (it->type == ITEM_INFO) ? NULL : "Change", it->type == ITEM_SLIDER, any_changed);
    } else {
        draw_hints("Open", false, any_changed);
    }

    pick_draw();

    if (confirm_delete >= 0) {
        ui_components_messagebox_draw("Delete this profile?\n\n%s\n\n^%02XA: Delete^00    ^%02XB: Keep^00\nIts saves stay on the SD card.",
                                      profiles_name(confirm_delete), STL_RED, STL_GREEN);
    }

    letter_wheel_draw();    // naming a profile

    if (confirm_reset) {
        ui_components_messagebox_draw(
            "Reset settings?\n\n"
            "A: Yes, B: Back"
        );
    }

    if (notice) {
        ui_components_messagebox_draw("%s\n\nPress any button.", notice);
    }

    animations_draw_late(menu->mode);  // Showy: a pop-up just closed shrinks away
    debug_stats_overlay_draw(); // Performance Overlay, if it is on

    rdpq_detach_show();
}

void view_settings_menu_init (menu_t *menu) {
    (void) menu;
    confirm_reset = false;
    confirm_delete = -1;
    notice = NULL;
    picking = NULL;
    bool back_from_tool = tool_open;
    tool_open = false;
    rows_rise_pending = !back_from_tool;   /* Showy: the rows rise into place (draw()) */
    card_themes_rescan();   /* theme folders added on the PC since */

    // Start from the top, unless the player wants to come back to where they were
    // (or comes back from a screen opened here).
    if (!features_enabled(FEATURE_REMEMBER_SETTINGS) && !back_from_tool) {
        group = 0;
        category = 0;
        sub_page = NULL;
        item = 0;
        in_items = false;
        first_row = 0;
    }
}

void view_settings_menu_display (menu_t *menu, surface_t *display) {
    process(menu);
    if (menu->next_mode != MENU_MODE_SETTINGS_EDITOR) {
        assistant_portrait_free();  // the Fairy Look page's fairy
        art_report_cancel();    // leaving before the report is done
    }
    art_report_step(menu);      // one game a frame while a report is under way

    if (menu->next_mode != MENU_MODE_SETTINGS_EDITOR || (unsaved && (get_ticks_ms() - changed_at) >= IDLE_SAVE_MS)) {
        save_changes(menu);
    }

    draw(menu, display);
}
