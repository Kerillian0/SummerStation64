#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "assistant.h"
#include "builtin_themes.h"
#include "card_themes.h"
#include "fonts.h"
#include "games_ui.h"
#include "intro.h"
#include "letter_wheel.h"
#include "menu_features.h"
#include "menu_options.h"
#include "region_pick.h"
#include "safe_mode.h"
#include "screenshot.h"
#include "settings.h"
#include "setup_wizard.h"
#include "sound.h"
#include "theme.h"
#include "ui_components.h"
#include "ui_components/constants.h"

#define PREVIEW_MS      (300)   /* a highlighted theme is shown once the selection rests this long */

/* Where things are. */
#define FAIRY_X         (92)
#define FAIRY_Y         (112)
#define FAIRY_SIZE      (72)
#define PANEL_X0        (160)
#define PANEL_X1        (600)
#define PANEL_Y0        (36)
#define PANEL_Y1        (176)
#define PANEL_PAD       (12)
#define ROWS_X0         (200)
#define ROWS_X1         (560)
#define ROWS_Y          (188)
#define ROW_H           (26)
#define STEP_Y          (200)

typedef enum {
    STEP_WELCOME,
    STEP_CONSOLE,
    STEP_VIDEO,
    STEP_REGION,
    STEP_TAGS,
    STEP_COPIES,
    STEP_THEME,
    STEP_ANIMATIONS,
    STEP_STARTUP,
    STEP_OFTEN,
    STEP_MOOD,
    STEP_NAME,
    STEP_DONE,
    STEP_COUNT
} step_t;

#define QUESTIONS       (STEP_DONE - 1)     /* the pages between the welcome and the end */
#define ROWS_MAX        (8)

/* What Vee says on each page: eager, snarky (%n her name). */
static const char *const asks[STEP_COUNT][2] = {
    [STEP_WELCOME] = { "Hi! I'm %n, the menu's fairy. Let's set a few things up together. It only takes a minute!",
                       "Oh, hi. I'm %n. Let's set a few things up now, so you stop asking me later." },
    [STEP_CONSOLE] = { "First things first: what are you playing on?",
                       "Let's get going. What are you playing on?" },
    [STEP_VIDEO] = { "Which picture? I'd pick 240p for you.",
                     "240p or 480i? Take 240p. Trust me." },
    [STEP_REGION] = { "Where are most of your games from?",
                      "Where are most of your games from? Be honest." },
    [STEP_TAGS] = { "Game names often carry tags like (U) or [!]. Shall I hide them?",
                    "File names are full of (U) and [!] clutter. Hide it?" },
    [STEP_COPIES] = { "Got the same game from more than one region?",
                      "Collected a game from three regions? Thought so." },
    [STEP_THEME] = { "Pick a look! You'll see each one as you go.",
                     "Pick a look. Go on, I'll wait." },
    [STEP_ANIMATIONS] = { "Do you like a bit of sparkle when things move?",
                          "Plain and quick, or a bit of a show?" },
    [STEP_STARTUP] = { "How should I greet you when you switch on?",
                       "What should happen when you switch on?" },
    [STEP_OFTEN] = { "How often should I pop up with tips?",
                     "How often do you want to hear from me? Choose wisely." },
    [STEP_MOOD] = { "And how should I talk to you?",
                    "And how should I talk to you?" },
    [STEP_NAME] = { "One last thing: I'm %n. Happy with that, or shall I be someone else?",
                    "Last one: I'm %n. Keep it, or rename me. I'll cope." },
    [STEP_DONE] = { "All set! Have fun! START opens Settings any time, if you change your mind.",
                    "Done. Finally. START opens Settings if you change your mind." },
};

/* The answers on the page shown: what each says, what it does, and its value. */
static struct {
    const char *label;
    const char *about;
    int value;
} rows[ROWS_MAX];
static int row_count = 0;
static int selected = 0;
static step_t step = STEP_WELCOME;

static bool active = false;         /* running inside Settings */
static bool showing = false;        /* running on the start-up screen, first time */
static menu_mode_t after;           /* where the start-up screen goes once it is done */
static bool video_was = false;      /* force_progressive_scan when it opened: changed means a restart */
static int theme_entered = 0;       /* option `theme` when the theme page opened (B puts it back) */
static int theme_shown = 0;         /* the theme on screen now */
static uint64_t moved_at = 0;
static char keep_name[48];
static char keep_theme[48];

static void apply_theme (int value) {
    if (value == theme_shown) {
        return;
    }
    options_change(OPTION_THEME, value);
    card_themes_note_choice();
    theme_reload();
    sound_reload_files();
    fonts_restyle();
    theme_shown = value;
}

static void add (const char *label, const char *about, int value) {
    if (row_count < ROWS_MAX) {
        rows[row_count].label = label;
        rows[row_count].about = about;
        rows[row_count].value = value;
        row_count++;
    }
}

static void select_value (int value) {
    selected = 0;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].value == value) {
            selected = i;
        }
    }
}

/* Fill the page's answers, the present value selected. */
static void fill (menu_t *menu) {
    row_count = 0;
    switch (step) {
        case STEP_WELCOME:
            add("Let's go!", "A answers and goes on, B goes back a page, and START keeps the rest as it is. Everything here is in Settings too.", 1);
            add("Skip setup", "Keep everything as it is. You can run this again from Settings > Console > About > Setup Wizard.", 0);
            selected = 0;
            break;
        case STEP_CONSOLE:
            add("A CRT TV", "An N64 on a tube TV, by composite, S-Video or RGB.", CONSOLE_TYPE_CRT);
            add("An HDMI-modded N64", "An N64 with an HDMI mod, or through an upscaler such as a RetroTINK or an OSSC.", CONSOLE_TYPE_HDMI_MOD);
            add("Analogue 3D", "The Analogue 3D: it upscales the picture itself.", CONSOLE_TYPE_ANALOGUE_3D);
            add("ModRetro M64", "The ModRetro M64: it upscales the picture itself.", CONSOLE_TYPE_MODRETRO_M64);
            select_value(options_get(OPTION_CONSOLE_TYPE));
            break;
        case STEP_VIDEO:
            add("240p", "Steady, with no flicker: what CRTs, upscalers and HDMI mods handle best.", 1);
            add("480i", "Twice the lines, for sharper text, but it can flicker on a CRT and look softer through an upscaler.", 0);
            selected = 0;   /* 240p is suggested whatever the console */
            break;
        case STEP_REGION:
            add("USA", "Your covers and names lean to USA copies. With one cover per game, coming up, the USA copy gives it.", PREFERRED_USA);
            add("Japan", "Your covers lean to Japanese copies. With one cover per game, coming up, the Japanese copy gives it.", PREFERRED_JAPAN);
            add("Europe", "Your covers lean to European copies. With one cover per game, coming up, the European copy gives it.", PREFERRED_EUROPE);
            select_value(options_get(OPTION_PREFERRED_REGION));
            break;
        case STEP_TAGS:
            add("Hide them", "Tidier names. Two regions of one game then look alike, but where you pick between them the region still shows.", 1);
            add("Show them", "Names just as the files are named, tags and all.", 0);
            select_value(features_enabled(FEATURE_HIDE_TAGS) ? 1 : 0);
            break;
        case STEP_COPIES:
            add("One cover per game", "Each game shows once, as your region's copy. A on it lets you pick another copy.", REGION_COPIES_ONE_COVER);
            add("Every copy its own cover", "Every file gets its own cover, side by side.", REGION_COPIES_SEPARATE);
            select_value(options_get(OPTION_REGION_COPIES));
            break;
        case STEP_THEME: {
            static const char *const about = "Settings > Look > Theme has more, and Edit Theme there makes your own.";
            int now = options_get(OPTION_THEME);
            if (now >= THEME_CHOICE_CARD) {
                snprintf(keep_theme, sizeof(keep_theme), "Keep %s", card_themes_choice_name(now));
                add(keep_theme, "The theme you have now, from your SD card.", now);
            }
            /* Midnight is the menu's own: "From SD Card" shows it when the card has no theme of its own. */
            add("Midnight", about, (now == BUILTIN_THEME_FROM_CARD) ? BUILTIN_THEME_FROM_CARD : BUILTIN_THEME_MIDNIGHT);
            add("Sunset", about, BUILTIN_THEME_SUNSET);
            add("Beach", about, BUILTIN_THEME_BEACH);
            add("Ocean", about, BUILTIN_THEME_OCEAN);
            add("Mint Grid", about, BUILTIN_THEME_MINT_GRID);
            add("Grape", about, BUILTIN_THEME_GRAPE);
            select_value(now);
            theme_entered = theme_shown = now;
            break;
        }
        case STEP_ANIMATIONS:
            add("Snappy", "Quick and plain: things change at once.", ANIMATIONS_SNAPPY);
            add("Showy", "Covers glide and pop, pop-ups grow, screens slide in. A bit more going on.", ANIMATIONS_SHOWY);
            select_value(options_get(OPTION_ANIMATIONS));
            break;
        case STEP_STARTUP: {
            add("Intro, then fade in", "The logo and title with a little tune, then the menu fades up.", 0);
            add("Just fade in", "No intro: the menu fades up from black.", 1);
            add("Straight in", "The menu, at once.", 2);
            bool intro = options_get(OPTION_INTRO) != INTRO_OFF;
            select_value(intro ? 0 : (options_get(OPTION_FADE) != FADE_OFF) ? 1 : 2);
            break;
        }
        case STEP_OFTEN:
            add("Rarely", "Now and again, a tip about something you haven't tried yet.", ASSISTANT_RARELY);
            add("Now and then", "Tips, and a word now and then about the game you rest on.", ASSISTANT_NOW_AND_THEN);
            add("Chatty", "Something about nearly every game you stop on.", ASSISTANT_CHATTY);
            add("Off", "Not a peep. Ask a Question, in Settings, still works.", ASSISTANT_OFF);
            select_value(options_get(OPTION_ASSISTANT));
            break;
        case STEP_MOOD:
            add("Eager", "\"Ooh, good question!\" Cheerful and keen to help.", ASSISTANT_EAGER);
            add("Snarky", "\"Ugh, fine. Listen closely.\" Teasing, all in fun.", ASSISTANT_SNARKY);
            select_value(options_get(OPTION_ASSISTANT_MOOD));
            break;
        case STEP_NAME:
            snprintf(keep_name, sizeof(keep_name), "Keep \"%s\"", assistant_name());
            add(keep_name, "Settings > Look > Assistant > Fairy Look changes it, my colour and how I fly in, any time.", 0);
            add("Give me a new name...", "Type it with the keyboard: START keeps it, Z goes back.", 1);
            selected = 0;
            break;
        case STEP_DONE:
            add(showing ? "Start playing!" : "Back to Settings",
                (menu->settings.force_progressive_scan != video_was)
                    ? "Remember: switch the console off and on again for the new picture."
                    : "Your games are waiting.", 0);
            selected = 0;
            break;
        default:
            break;
    }
    moved_at = get_ticks_ms();
}

static void begin (menu_t *menu) {
    step = STEP_WELCOME;
    video_was = menu->settings.force_progressive_scan;
    fill(menu);
}

void setup_wizard_begin_first_run (menu_t *menu) {
    if (!menu->settings.first_run || safe_mode_active()) {
        return;
    }
    after = intro_hold(menu);
    showing = true;
    begin(menu);
    debugf("setup wizard: first start, asking\n");
}

void setup_wizard_open (menu_t *menu) {
    active = true;
    begin(menu);
    sound_play_effect(SFX_ENTER);
}

bool setup_wizard_active (void) {
    return active;
}

/* Save what was chosen and leave. */
static void finish (menu_t *menu) {
    if (step == STEP_THEME) {
        apply_theme(rows[selected].value);
    }
    options_flush();
    features_user_flush();
    menu->settings.first_run = false;
    settings_save(&menu->settings);
    debugf("setup wizard: done (console %d, %s, region %d, theme %d)\n", options_get(OPTION_CONSOLE_TYPE),
        menu->settings.force_progressive_scan ? "240p" : "480i", options_get(OPTION_PREFERRED_REGION), options_get(OPTION_THEME));
    if (showing) {
        showing = false;
        menu->next_mode = after;    /* the first tab, which fades in now */
    }
    active = false;
}

static void name_given (menu_t *menu, const char *name) {
    if (name) {
        char kept[ASSISTANT_NAME_MAX + 1];
        snprintf(kept, sizeof(kept), "%s", name);
        assistant_rename(kept);
        sound_play_effect(SFX_SETTING);
        step = STEP_DONE;
        fill(menu);
    }
}

/* The answer chosen on this page takes effect. */
static void answer (menu_t *menu, int value) {
    switch (step) {
        case STEP_CONSOLE:
            options_change(OPTION_CONSOLE_TYPE, value);
            break;
        case STEP_VIDEO:
            menu->settings.force_progressive_scan = (value != 0);
            break;
        case STEP_REGION:
            if (value != options_get(OPTION_PREFERRED_REGION)) {
                options_change(OPTION_PREFERRED_REGION, value);
                region_pick_forget_all();
                menu->browser.reload = true;
            }
            break;
        case STEP_TAGS:
            if ((value != 0) != features_enabled(FEATURE_HIDE_TAGS)) {
                features_user_change(FEATURE_HIDE_TAGS, value);
                menu->browser.reload = true;
            }
            break;
        case STEP_COPIES:
            if (value != options_get(OPTION_REGION_COPIES)) {
                options_change(OPTION_REGION_COPIES, value);
                menu->browser.reload = true;
            }
            break;
        case STEP_THEME:
            apply_theme(value);
            break;
        case STEP_ANIMATIONS:
            options_change(OPTION_ANIMATIONS, value);
            break;
        case STEP_STARTUP: {
            int intro = options_get(OPTION_INTRO);
            options_change(OPTION_INTRO, (value == 0) ? ((intro == INTRO_OFF) ? INTRO_ON : intro) : INTRO_OFF);
            int fade = options_get(OPTION_FADE);
            options_change(OPTION_FADE, (value == 2) ? FADE_OFF : ((fade == FADE_OFF) ? FADE_2_SECONDS : fade));
            break;
        }
        case STEP_OFTEN:
            options_change(OPTION_ASSISTANT, value);
            break;
        case STEP_MOOD:
            options_change(OPTION_ASSISTANT_MOOD, value);
            break;
        default:
            break;
    }
}

static void go (menu_t *menu, int way) {
    step_t next = (step_t) (step + way);
    bool quiet = options_get(OPTION_ASSISTANT) == ASSISTANT_OFF;
    while (quiet && (next == STEP_MOOD || next == STEP_NAME)) {
        next = (step_t) (next + way);     /* switched off: no mood or name to ask about */
    }
    if (next < STEP_WELCOME || next >= STEP_COUNT) {
        return;
    }
    step = next;
    fill(menu);
}

void setup_wizard_process (menu_t *menu) {
    if (letter_wheel_naming()) {
        letter_wheel_name_process(menu);    /* naming Vee */
        return;
    }
    if (menu->actions.go_up && selected > 0) {
        selected--;
        moved_at = get_ticks_ms();
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && selected < row_count - 1) {
        selected++;
        moved_at = get_ticks_ms();
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.settings && step != STEP_DONE) {
        if (step == STEP_THEME) {
            apply_theme(rows[selected].value);
        }
        step = STEP_DONE;   /* START: the rest stays as it is */
        fill(menu);
        sound_play_effect(SFX_ENTER);
    } else if (menu->actions.enter) {
        int value = rows[selected].value;
        sound_play_effect(SFX_ENTER);
        if (step == STEP_WELCOME && value == 0) {
            step = STEP_DONE;   /* Skip setup */
            fill(menu);
        } else if (step == STEP_NAME && value == 1) {
            letter_wheel_name_begin("My new name", NULL, name_given);
        } else if (step == STEP_DONE) {
            finish(menu);
        } else {
            answer(menu, value);
            go(menu, 1);
        }
    } else if (menu->actions.back) {
        if (step == STEP_THEME) {
            apply_theme(theme_entered);     /* not chosen: the look it had */
        }
        if (step == STEP_WELCOME) {
            if (active) {
                finish(menu);   /* from Settings: B leaves, nothing changed */
            }
        } else {
            go(menu, -1);
        }
        sound_play_effect(SFX_EXIT);
    }

    /* The theme page shows the highlighted look once the selection rests. */
    if (step == STEP_THEME && (get_ticks_ms() - moved_at) >= PREVIEW_MS) {
        apply_theme(rows[selected].value);
    }
}

/* ---------- drawing ---------- */

static void text (int x, int y, int width, int height, rdpq_align_t align, menu_font_style_t style, const char *string) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = width, .height = height, .align = align, .valign = VALIGN_TOP,
        .wrap = WRAP_WORD, .style_id = style }, FNT_DEFAULT, x, y, "%s", string);
}

/* Vee's line, her name in place of %n. */
static void line_for (char *out, size_t size, const char *line) {
    const char *at = strstr(line, "%n");
    if (!at) {
        snprintf(out, size, "%s", line);
        return;
    }
    snprintf(out, size, "%.*s%s%s", (int) (at - line), line, assistant_name(), at + 2);
}

void setup_wizard_draw (void) {
    if (letter_wheel_keyboard_showing()) {
        letter_wheel_draw();    /* the keyboard covers the page */
        return;
    }
    const theme_t *t = theme_get();

    /* Her speech panel, edged in her colour. */
    color_t edge = assistant_color();
    games_ui_round_fill(PANEL_X0 - 2, PANEL_Y0 - 2, PANEL_X1 + 2, PANEL_Y1 + 2, edge);
    games_ui_round_fill(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xFF));
    char name[32], ask[200];
    snprintf(name, sizeof(name), "%s:", assistant_name());
    text(PANEL_X0 + PANEL_PAD, PANEL_Y0 + 8, PANEL_X1 - PANEL_X0 - 2 * PANEL_PAD, 22, ALIGN_LEFT, STL_ORANGE, name);
    line_for(ask, sizeof(ask), asks[step][(options_get(OPTION_ASSISTANT_MOOD) == ASSISTANT_SNARKY) ? 1 : 0]);
    text(PANEL_X0 + PANEL_PAD, PANEL_Y0 + 30, PANEL_X1 - PANEL_X0 - 2 * PANEL_PAD, 48, ALIGN_LEFT, STL_DEFAULT, ask);
    if (row_count > 0) {
        text(PANEL_X0 + PANEL_PAD, PANEL_Y0 + 82, PANEL_X1 - PANEL_X0 - 2 * PANEL_PAD, PANEL_Y1 - PANEL_Y0 - 88, ALIGN_LEFT, STL_GRAY,
            rows[selected].about);
    }

    /* Vee herself, on the left. */
    assistant_portrait_draw(FAIRY_X, FAIRY_Y, FAIRY_SIZE);
    if (step > STEP_WELCOME && step < STEP_DONE) {
        char where[24];
        snprintf(where, sizeof(where), "%d of %d", (int) step, QUESTIONS);
        text(FAIRY_X - 60, STEP_Y, 120, 22, ALIGN_CENTER, STL_GRAY, where);
    }

    /* The answers. */
    games_ui_round_fill(ROWS_X0 - 8, ROWS_Y - 6, ROWS_X1 + 8, ROWS_Y + row_count * ROW_H + 6, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
    static animations_glide_t glide;
    int bar_y = animations_glide(&glide, ROWS_Y + selected * ROW_H);
    games_ui_round_fill(ROWS_X0 - 4, bar_y, ROWS_X1 + 4, bar_y + ROW_H, FILE_LIST_HIGHLIGHT_COLOR);
    for (int i = 0; i < row_count; i++) {
        rdpq_text_printf(&(rdpq_textparms_t) { .width = ROWS_X1 - ROWS_X0 - 20, .height = ROW_H, .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT }, FNT_DEFAULT, ROWS_X0 + 16, ROWS_Y + i * ROW_H, "%s", rows[i].label);
    }

    /* The picture only changes at the next start: a soft warning under the answers. */
    if (step == STEP_VIDEO) {
        int y0 = ROWS_Y + row_count * ROW_H + 16;
        games_ui_round_fill(ROWS_X0 - 8, y0, ROWS_X1 + 8, y0 + 50, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
        text(ROWS_X0 + 4, y0 + 4, ROWS_X1 - ROWS_X0 - 8, 44, ALIGN_LEFT, STL_YELLOW,
            "Either one only takes effect after you switch the console off and on again.");
    }

    games_ui_hints_backdrop_draw();
    int x = games_ui_hint_draw(GAMES_UI_HINTS_X, 1, "A", (step == STEP_DONE) ? "Done" : "Next");
    if (step > STEP_WELCOME || active) {
        x = games_ui_hint_draw(x, 1, "B", "Back");
    }
    if (step > STEP_WELCOME && step < STEP_DONE) {
        games_ui_hint_draw(x, 1, "START", "Skip the rest");
    }

    letter_wheel_draw();    /* the dial, if names are spelled with it */
}

bool setup_wizard_display (menu_t *menu, surface_t *display) {
    if (!showing) {
        return false;
    }
    setup_wizard_process(menu);
    intro_fade_wait();      /* the first tab's fade-in (and the music) start once it is done */
    if (!showing) {
        assistant_portrait_free();
    }
    rdpq_attach(display, NULL);
    ui_components_background_draw();
    if (showing) {
        setup_wizard_draw();
    }
    rdpq_detach_show();
    return true;
}
