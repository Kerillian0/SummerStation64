#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "assistant.h"
#include "fonts.h"
#include "games_ui.h"
#include "intro.h"
#include "menu_options.h"
#include "safe_mode.h"
#include "sound.h"
#include "theme.h"
#include "ui_components.h"
#include "ui_components/constants.h"
#include "whats_new.h"

#define LIST_PATH       "rom:/whatsnew.txt"
#define LIST_MAX_BYTES  (16 * 1024)
#define ITEMS_MAX       (48)

/* Where things are: as the setup wizard. */
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
#define ROWS_SHOWN      (7)

typedef struct {
    const char *name;
    const char *go;         /* NULL: nothing to show */
    const char *about;
    const char *version;    /* "0.5" */
} item_t;

static char *text = NULL;           /* the file, its lines cut in place */
static item_t items[ITEMS_MAX];
static int item_count = 0;
static int newest = 0;              /* the newest version in the file, as a number */
static const char *newest_name = "";
static bool several_versions = false;
static int selected = 0, first = 0;

static bool showing = false;        /* on the start-up screen */
static bool active = false;         /* inside Settings */
static bool from_startup = false;   /* went on into Settings with "Show me" from the start-up screen */
static menu_mode_t after;
static const char *jump = NULL;
static bool started = false;        /* its first frame on the start-up screen: a press that skipped the intro isn't for it */

/* "0.5" -> 500, "0.4a" -> 401, "1.0" -> 10000. */
static int version_number (const char *v) {
    int major = 0, minor = 0;
    char letter = 0;
    if (sscanf(v, "%d.%d%c", &major, &minor, &letter) < 2) {
        return 0;
    }
    return major * 10000 + minor * 100 + ((letter >= 'a' && letter <= 'z') ? letter - 'a' + 1 : 0);
}

/* Read the file and keep the items of every version newer than `since` (all of the newest at least). */
static void load (int since) {
    free(text);
    text = NULL;
    item_count = 0;
    newest = 0;
    newest_name = "";
    several_versions = false;
    FILE *f = fopen(LIST_PATH, "r");
    if (!f) {
        debugf("whats new: %s missing\n", LIST_PATH);
        return;
    }
    text = malloc(LIST_MAX_BYTES + 1);
    if (!text) {
        fclose(f);
        return;
    }
    size_t n = fread(text, 1, LIST_MAX_BYTES, f);
    fclose(f);
    text[n] = '\0';

    const char *version = NULL;
    bool keep = false;
    int versions_kept = 0;
    for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        if (strncmp(line, "version ", 8) == 0) {
            version = line + 8;
            int number = version_number(version);
            if (newest == 0) {
                newest = number;
                newest_name = version;
            }
            keep = (number > since) || (number == newest);
            versions_kept += keep ? 1 : 0;
        } else if (strncmp(line, "item ", 5) == 0 && keep && item_count < ITEMS_MAX) {
            items[item_count++] = (item_t) { .name = line + 5, .version = version };
        } else if (strncmp(line, "go ", 3) == 0 && keep && item_count > 0) {
            items[item_count - 1].go = line + 3;
        } else if (strncmp(line, "about ", 6) == 0 && keep && item_count > 0) {
            items[item_count - 1].about = line + 6;
        }
    }
    several_versions = versions_kept > 1;
    selected = first = 0;
}

static void forget (void) {
    free(text);
    text = NULL;
    item_count = 0;
    assistant_portrait_free();
}

/* The newest version in the file, without keeping the list. */
static int newest_in_file (void) {
    load(1000000);
    int v = newest;
    forget();
    return v;
}

void whats_new_begin_startup (menu_t *menu) {
    if (safe_mode_active()) {
        return;
    }
    int seen = options_get(OPTION_WHATS_NEW_SEEN);
    if (menu->settings.first_run) {
        /* A new card: the setup wizard greets it; there is nothing "new" to it. */
        int v = newest_in_file();
        if (v != seen) {
            options_set(OPTION_WHATS_NEW_SEEN, v);
        }
        return;
    }
    if (newest_in_file() <= seen) {
        return;
    }
    load(seen);
    if (item_count == 0) {
        forget();
        return;
    }
    after = intro_hold(menu);
    showing = true;
    started = false;
    debugf("whats new: %d items since version %d\n", item_count, seen);
}

void whats_new_open (menu_t *menu) {
    (void) menu;
    load(options_get(OPTION_WHATS_NEW_SEEN));
    active = item_count > 0;
    sound_play_effect(SFX_ENTER);
}

bool whats_new_active (void) {
    return active;
}

const char *whats_new_take_jump (void) {
    const char *go = jump;
    jump = NULL;
    return go;
}

void whats_new_resume (void) {
    active = true;
}

static void finish (menu_t *menu) {
    if (options_get(OPTION_WHATS_NEW_SEEN) != newest) {
        options_set(OPTION_WHATS_NEW_SEEN, newest);
    }
    forget();
    if (showing) {
        showing = false;
        menu->next_mode = after;    /* the first tab, which fades in now */
    } else if (from_startup) {
        from_startup = false;
        menu->next_mode = after;    /* went on into Settings from the start-up screen: back to the first tab */
    }
    active = false;
}

void whats_new_process (menu_t *menu) {
    if (menu->actions.go_up && selected > 0) {
        selected--;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && selected < item_count - 1) {
        selected++;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter && item_count > 0 && items[selected].go) {
        jump = items[selected].go;      /* Settings goes there (whats_new_take_jump()) */
        if (options_get(OPTION_WHATS_NEW_SEEN) != newest) {
            options_set(OPTION_WHATS_NEW_SEEN, newest);     /* seen, even if Settings is left another way */
        }
        sound_play_effect(SFX_ENTER);
        if (showing) {
            showing = false;
            from_startup = true;
            menu->next_mode = MENU_MODE_SETTINGS_EDITOR;
        }
        active = false;
    } else if (menu->actions.back) {
        finish(menu);
        sound_play_effect(SFX_EXIT);
    }
    if (selected < first) {
        first = selected;
    } else if (selected >= first + ROWS_SHOWN) {
        first = selected - ROWS_SHOWN + 1;
    }
}

/* ---------- drawing ---------- */

static void text_box (int x, int y, int width, int height, rdpq_align_t align, menu_font_style_t style, const char *string) {
    rdpq_text_printf(&(rdpq_textparms_t) { .width = width, .height = height, .align = align, .valign = VALIGN_TOP,
        .wrap = WRAP_WORD, .style_id = style }, FNT_DEFAULT, x, y, "%s", string);
}

void whats_new_draw (void) {
    if (item_count == 0) {
        return;
    }
    const theme_t *t = theme_get();
    const item_t *it = &items[selected];

    /* Her speech panel, edged in her colour. */
    games_ui_round_fill(PANEL_X0 - 2, PANEL_Y0 - 2, PANEL_X1 + 2, PANEL_Y1 + 2, assistant_color());
    games_ui_round_fill(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xFF));
    char line[96];
    snprintf(line, sizeof(line), "%s:", assistant_name());
    text_box(PANEL_X0 + PANEL_PAD, PANEL_Y0 + 8, PANEL_X1 - PANEL_X0 - 2 * PANEL_PAD, 22, ALIGN_LEFT, STL_ORANGE, line);
    if (assistant_snarky()) {
        snprintf(line, sizeof(line), "Here's what changed in v%s. Try to keep up.", newest_name);
    } else {
        snprintf(line, sizeof(line), "Here's what's new in v%s!", newest_name);
    }
    text_box(PANEL_X0 + PANEL_PAD, PANEL_Y0 + 30, PANEL_X1 - PANEL_X0 - 2 * PANEL_PAD, 24, ALIGN_LEFT, STL_DEFAULT, line);
    text_box(PANEL_X0 + PANEL_PAD, PANEL_Y0 + 56, PANEL_X1 - PANEL_X0 - 2 * PANEL_PAD, PANEL_Y1 - PANEL_Y0 - 62, ALIGN_LEFT, STL_GRAY,
        it->about ? it->about : "");

    assistant_portrait_draw(FAIRY_X, FAIRY_Y, FAIRY_SIZE);
    snprintf(line, sizeof(line), "%d of %d", selected + 1, item_count);
    text_box(FAIRY_X - 60, 200, 120, 22, ALIGN_CENTER, STL_GRAY, line);

    /* The list. */
    int shown = (item_count < ROWS_SHOWN) ? item_count : ROWS_SHOWN;
    games_ui_round_fill(ROWS_X0 - 8, ROWS_Y - 6, ROWS_X1 + 8, ROWS_Y + shown * ROW_H + 6, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xD8));
    static animations_glide_t glide;
    int bar_y = animations_glide(&glide, ROWS_Y + (selected - first) * ROW_H);
    games_ui_round_fill(ROWS_X0 - 4, bar_y, ROWS_X1 + 4, bar_y + ROW_H, FILE_LIST_HIGHLIGHT_COLOR);
    for (int i = first; i < first + shown; i++) {
        int y = ROWS_Y + (i - first) * ROW_H;
        rdpq_text_printf(&(rdpq_textparms_t) { .width = ROWS_X1 - ROWS_X0 - 80, .height = ROW_H, .valign = VALIGN_CENTER,
            .wrap = WRAP_ELLIPSES, .style_id = STL_DEFAULT }, FNT_DEFAULT, ROWS_X0 + 16, y, "%s", items[i].name);
        if (several_versions) {
            char v[16];
            snprintf(v, sizeof(v), "v%s", items[i].version);
            rdpq_text_printf(&(rdpq_textparms_t) { .width = 60, .height = ROW_H, .align = ALIGN_RIGHT, .valign = VALIGN_CENTER,
                .style_id = STL_GRAY }, FNT_DEFAULT, ROWS_X1 - 64, y, "%s", v);
        }
    }
    if (item_count > ROWS_SHOWN) {
        /* A thin bar at the right: how far down the list is. */
        int track = ROWS_SHOWN * ROW_H;
        int thumb = (track * ROWS_SHOWN) / item_count;
        int thumb_y = ROWS_Y + ((track - thumb) * first) / (item_count - ROWS_SHOWN);
        rdpq_set_mode_fill(RGBA32(t->tab_inactive.r, t->tab_inactive.g, t->tab_inactive.b, 0xFF));
        rdpq_fill_rectangle(ROWS_X1 + 12, ROWS_Y, ROWS_X1 + 16, ROWS_Y + track);
        rdpq_set_mode_fill(RGBA32(t->accent.r, t->accent.g, t->accent.b, 0xFF));
        rdpq_fill_rectangle(ROWS_X1 + 12, thumb_y, ROWS_X1 + 16, thumb_y + thumb);
    }

    games_ui_hints_backdrop_draw();
    int x = GAMES_UI_HINTS_X;
    if (it->go) {
        x = games_ui_hint_draw(x, 1, "A", "Show me");
    }
    games_ui_hint_draw(x, 1, "B", "Done");
}

bool whats_new_display (menu_t *menu, surface_t *display) {
    if (!showing) {
        return false;
    }
    if (started) {
        whats_new_process(menu);
    }
    started = true;
    intro_fade_wait();      /* the first tab's fade-in (and the music) start once it is done */
    rdpq_attach(display, NULL);
    ui_components_background_draw();
    if (showing) {
        whats_new_draw();
    }
    rdpq_detach_show();
    return true;
}
