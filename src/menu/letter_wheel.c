#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libdragon.h>

#include "cover_list.h"
#include "display_name.h"
#include "fonts.h"
#include "letter_wheel.h"
#include "sound.h"
#include "theme.h"
#include "ui_components/constants.h"

#define OPEN_AFTER_MS   (300)   /* Z held this long opens the dial; let go sooner, it is a tap */
#define SLOTS           (27)    /* #, then A-Z, round the dial clockwise from the top */
#define KEY_LENGTH      (24)    /* letters of each name kept for matching */
#define PREFIX_MAX      (12)
#define STICK_DEADZONE  (40)    /* how far the stick must lean to point at a letter (it reads about +-85) */

#define CENTER_X        (DISPLAY_CENTER_X)
#define CENTER_Y        (176)   /* over the cover row */
#define DISC_RADIUS     (128)
#define LETTER_RADIUS   (100)
#define MARK_RADIUS     (15)    /* the accent disc behind the letter pointed at */

static bool z_down = false;     /* Z went down on a screen with the dial and hasn't come up yet */
static uint64_t z_down_at = 0;
static bool open = false;
static char (*keys)[KEY_LENGTH] = NULL;    /* each entry's name as matched, by place in the row */
static int key_count = 0;
static char prefix[PREFIX_MAX + 1] = "";
static int pointed = 1;         /* the slot pointed at (1 = A) */
static bool has_games[SLOTS];   /* slots that some name has after the prefix */
static int start_selected = 0;  /* where the dial opened, for B with nothing typed */
static sprite_t *disc = NULL;

/* Naming something (letter_wheel_name_begin()): the dial stays open by
   itself and spells a name: Space and A-Z, or 0-9 after C-up / C-down. */
#define NAME_MAX        (27)
#define DIGIT_SLOTS     (10)
static bool naming = false;
static bool digits = false;
static char name_text[NAME_MAX + 1];
static const char *name_prompt = "";
static void (*name_done) (menu_t *menu, const char *name) = NULL;

static int slot_total (void) {
    return (naming && digits) ? DIGIT_SLOTS : SLOTS;
}

static char slot_char (int slot) {
    if (naming) {
        return digits ? (char) ('0' + slot) : ((slot == 0) ? ' ' : (char) ('A' + slot - 1));
    }
    return (slot == 0) ? '#' : (char) ('A' + slot - 1);
}

/* The stick's direction as a slot: up is the first, then clockwise. -1 while it is near the middle. */
static int stick_slot (joypad_inputs_t in) {
    int x = in.stick_x, y = in.stick_y;
    if ((x * x) + (y * y) < STICK_DEADZONE * STICK_DEADZONE) {
        return -1;
    }
    float turn = atan2f((float) x, (float) y);      /* 0 straight up, growing clockwise */
    if (turn < 0) {
        turn += 2.0f * (float) M_PI;
    }
    return ((int) lroundf(turn / (2.0f * (float) M_PI / slot_total()))) % slot_total();
}

/* "The Legend of Zelda - Ocarina of Time" -> "LEGENDOFZELDAOCARINAOFTIME". */
static void make_key (const char *name, char *key) {
    static const char *const articles[] = { "The ", "An ", "A " };
    for (int i = 0; i < 3; i++) {
        size_t n = strlen(articles[i]);
        if (strncasecmp(name, articles[i], n) == 0 && name[n]) {
            name += n;
            break;
        }
    }
    int at = 0;
    for (const unsigned char *c = (const unsigned char *) name; *c && at < KEY_LENGTH - 1; c++) {
        if (isalnum(*c)) {
            key[at++] = (char) toupper(*c);
        }
    }
    key[at] = '\0';
}

/* Does this key go on with `ch` at place `at` ('#' for any digit)? */
static bool key_has (const char *key, int at, char ch) {
    char k = key[at];
    return (ch == '#') ? (k >= '0' && k <= '9') : (k == ch);
}

static bool key_matches (const char *key, const char *typed, char next) {
    int n = (int) strlen(typed);
    for (int i = 0; i < n; i++) {
        if (!key_has(key, i, typed[i])) {
            return false;
        }
    }
    return next ? key_has(key, n, next) : true;
}

/* Which slots some name has after the prefix. */
static void find_slots (void) {
    for (int s = 0; s < SLOTS; s++) {
        has_games[s] = false;
        for (int i = 0; i < key_count && !has_games[s]; i++) {
            has_games[s] = key_matches(keys[i], prefix, slot_char(s));
        }
    }
}

static int first_match (char next) {
    for (int i = 0; i < key_count; i++) {
        if (key_matches(keys[i], prefix, next)) {
            return i;
        }
    }
    return -1;
}

static void select_entry (menu_t *menu, int index) {
    if (index < 0 || index >= menu->browser.entries || index == menu->browser.selected) {
        return;
    }
    menu->browser.selected = index;
    menu->browser.entry = &menu->browser.list[index];
    sound_play_effect(SFX_CURSOR);
}

/* The covers go to the first game with the prefix and the letter pointed at. */
static void jump (menu_t *menu) {
    if (has_games[pointed]) {
        select_entry(menu, first_match(slot_char(pointed)));
    }
}

void letter_wheel_close (void) {
    free(keys);
    keys = NULL;
    key_count = 0;
    open = false;
    z_down = false;
}

static bool open_wheel (menu_t *menu) {
    key_count = menu->browser.entries;
    keys = malloc(key_count * KEY_LENGTH);
    if (!keys) {
        key_count = 0;
        return false;
    }
    for (int i = 0; i < key_count; i++) {
        make_key(display_name(&menu->browser.list[i]), keys[i]);
    }
    prefix[0] = '\0';
    start_selected = menu->browser.selected;
    find_slots();
    /* Start at the selected game's own first letter. */
    pointed = 1;
    if (menu->browser.selected >= 0 && menu->browser.selected < key_count) {
        char first = keys[menu->browser.selected][0];
        pointed = (first >= 'A' && first <= 'Z') ? (first - 'A' + 1) : 0;
    }
    if (!disc) {
        disc = sprite_load("rom:/corner.sprite");
    }
    open = true;
    debugf("letter wheel: open, %d games\n", key_count);
    return true;
}

/* The next slot round the dial that has games, in `step`'s direction. */
static int next_slot (int from, int step) {
    for (int i = 1; i <= SLOTS; i++) {
        int s = (from + (step * i) + SLOTS * 2) % SLOTS;
        if (has_games[s]) {
            return s;
        }
    }
    return from;
}

static void handle (menu_t *menu, joypad_buttons_t pressed, joypad_inputs_t in) {
    /* The stick points round the dial: up is #, then clockwise. While it is
       held it has the last word on where the pointer is. */
    int s = stick_slot(in);
    bool leaning = (s >= 0);
    if (leaning) {
        if (s != pointed) {
            pointed = s;
            jump(menu);
        }
    }
    if (!leaning && (pressed.c_right || pressed.c_left)) {
        int s = next_slot(pointed, pressed.c_right ? 1 : -1);
        if (s != pointed) {
            pointed = s;
            jump(menu);
        }
    }
    if (pressed.a && has_games[pointed] && strlen(prefix) < PREFIX_MAX) {
        int n = (int) strlen(prefix);
        prefix[n] = slot_char(pointed);
        prefix[n + 1] = '\0';
        find_slots();
        select_entry(menu, first_match(0));
        if (!has_games[pointed] && !leaning) {
            pointed = next_slot(pointed, 1);    /* on to a letter that can follow, unless the stick points */
        }
        sound_play_effect(SFX_SETTING);
    } else if (pressed.b) {
        int n = (int) strlen(prefix);
        if (n > 0) {
            pointed = (prefix[n - 1] == '#') ? 0 : (prefix[n - 1] - 'A' + 1);
            prefix[n - 1] = '\0';
            find_slots();
            jump(menu);
        } else {
            select_entry(menu, start_selected);   /* nothing typed: back where it opened */
        }
        sound_play_effect(SFX_EXIT);
    }
}

letter_wheel_state_t letter_wheel_process (menu_t *menu) {
    const cover_list_t *covers = cover_list_current(menu);
    if (!covers || covers->list != menu->browser.list || menu->browser.entries < 2) {
        if (open) {
            letter_wheel_close();
        }
        z_down = false;
        return LETTER_WHEEL_IDLE;   /* not the Games list's covers: Z as usual */
    }

    joypad_buttons_t pressed = {0}, held = {0};
    joypad_inputs_t in = {0};
    JOYPAD_PORT_FOREACH (i) {
        /* The current state: "held" is still false on the frame a button goes down. */
        joypad_buttons_t p = joypad_get_buttons_pressed(i), h = joypad_get_buttons(i);
        if (p.raw || h.raw) {
            pressed = p;
            held = h;
            in = joypad_get_inputs(i);
            break;
        }
    }

    if (!z_down) {
        if (!pressed.z) {
            return LETTER_WHEEL_IDLE;
        }
        z_down = true;
        z_down_at = get_ticks_ms();
    }
    if (!held.z) {
        bool was_open = open;
        letter_wheel_close();
        return was_open ? LETTER_WHEEL_BUSY : LETTER_WHEEL_TAP;
    }
    if (!open) {
        bool leaning = (in.stick_x * in.stick_x) + (in.stick_y * in.stick_y) >= STICK_DEADZONE * STICK_DEADZONE;
        if ((get_ticks_ms() - z_down_at) < OPEN_AFTER_MS && !leaning && !pressed.c_left && !pressed.c_right) {
            return LETTER_WHEEL_BUSY;   /* a tap or a hold: not known yet */
        }
        if (!open_wheel(menu)) {
            return LETTER_WHEEL_BUSY;
        }
        sound_play_effect(SFX_ENTER);
    }
    handle(menu, pressed, in);
    return LETTER_WHEEL_BUSY;
}

/* Text centered on (x, y), in a box a line of its font high (rdpq drops
   text whose box is lower than a line: the title font needs 36). */
static void letter (int x, int y, int width, menu_font_style_t style, int font, const char *text) {
    int height = (font == FNT_TITLE) ? 40 : 32;
    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = width,
        .height = height,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .style_id = style,
    }, font, x - (width / 2), y - (height / 2), "%s", text);
}

/* The menu's small white disc (rom:/corner.sprite), stretched and tinted. */
static void disc_draw (int x, int y, int radius, color_t color) {
    if (!disc) {
        return;
    }
    float scale = (2.0f * radius) / disc->width;
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_set_prim_color(color);
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_sprite_blit(disc, x - radius, y - radius, &(rdpq_blitparms_t) { .scale_x = scale, .scale_y = scale });
    rdpq_mode_pop();
}

/* "COUCH CO OP" as typed -> "Couch Co Op". */
static void title_case (const char *in, char *out, size_t size) {
    size_t at = 0;
    bool word_start = true;
    for (const char *c = in; *c && at + 1 < size; c++) {
        char ch = *c;
        out[at++] = word_start ? (char) toupper((unsigned char) ch) : (char) tolower((unsigned char) ch);
        word_start = (ch == ' ');
    }
    out[at] = '\0';
}

void letter_wheel_name_begin (const char *prompt, const char *start, void (*done) (menu_t *menu, const char *name)) {
    snprintf(name_text, sizeof(name_text), "%s", start ? start : "");
    for (char *c = name_text; *c; c++) {
        *c = (char) toupper((unsigned char) *c);
    }
    name_prompt = prompt ? prompt : "";
    name_done = done;
    digits = false;
    pointed = 1;
    naming = true;
    if (!disc) {
        disc = sprite_load("rom:/corner.sprite");
    }
}

bool letter_wheel_naming (void) {
    return naming;
}

void letter_wheel_name_process (menu_t *menu) {
    if (!naming) {
        return;
    }
    joypad_buttons_t pressed = {0};
    joypad_inputs_t in = {0};
    JOYPAD_PORT_FOREACH (i) {
        joypad_buttons_t p = joypad_get_buttons_pressed(i), h = joypad_get_buttons(i);
        if (p.raw || h.raw) {
            pressed = p;
            in = joypad_get_inputs(i);
            break;
        }
    }
    int s = stick_slot(in);
    if (s >= 0) {
        pointed = s;    /* the stick has the last word while it leans */
    } else if (pressed.c_right || pressed.c_left) {
        pointed = (pointed + (pressed.c_right ? 1 : -1) + slot_total()) % slot_total();
        sound_play_effect(SFX_CURSOR);
    }
    if (pressed.c_up || pressed.c_down) {
        digits = !digits;
        pointed = digits ? 0 : 1;
        sound_play_effect(SFX_CURSOR);
    }
    int n = (int) strlen(name_text);
    if (pressed.a && n < NAME_MAX && !(n == 0 && slot_char(pointed) == ' ')) {
        name_text[n] = slot_char(pointed);
        name_text[n + 1] = '\0';
        sound_play_effect(SFX_SETTING);
    } else if (pressed.b) {
        sound_play_effect(SFX_EXIT);
        if (n > 0) {
            name_text[n - 1] = '\0';
        } else {
            naming = false;     /* nothing typed: give up */
            if (name_done) name_done(menu, NULL);
        }
    } else if (pressed.start && n > 0) {
        while (n > 0 && name_text[n - 1] == ' ') {
            name_text[--n] = '\0';
        }
        char name[NAME_MAX + 1];
        title_case(name_text, name, sizeof(name));
        naming = false;
        sound_play_effect(SFX_ENTER);
        if (name_done) name_done(menu, name);
    }
}

static void ring_draw (const theme_t *t) {
    for (int s = 0; s < slot_total(); s++) {
        float turn = s * (2.0f * (float) M_PI / slot_total());
        int x = CENTER_X + (int) lroundf(sinf(turn) * LETTER_RADIUS);
        int y = (CENTER_Y - (int) lroundf(cosf(turn) * LETTER_RADIUS)) & ~1;   /* even lines: steady on 240p */
        char text[4] = { slot_char(s), '\0' };
        if (text[0] == ' ') {
            snprintf(text, sizeof(text), "Spc");
        }
        if (s == pointed) {
            disc_draw(x, y, MARK_RADIUS, t->accent);
            letter(x, y, 30, STL_WHITE, FNT_DEFAULT, text);
        } else {
            letter(x, y, 30, (naming || has_games[s]) ? STL_DEFAULT : STL_GRAY, FNT_DEFAULT, text);
        }
    }
}

void letter_wheel_draw (void) {
    if (!open && !naming) {
        return;
    }
    const theme_t *t = theme_get();
    disc_draw(CENTER_X, CENTER_Y, DISC_RADIUS, RGBA32(t->panel.r, t->panel.g, t->panel.b, 0xE8));
    ring_draw(t);

    if (naming) {
        /* In the middle: what it is for, the name so far (its end, if long), and the buttons. */
        char shown[NAME_MAX + 1];
        title_case(name_text, shown, sizeof(shown));
        int n = (int) strlen(shown);
        const char *tail = (n > 14) ? shown + n - 14 : shown;
        letter(CENTER_X, CENTER_Y - 44, 170, STL_GRAY, FNT_DEFAULT, name_prompt);
        letter(CENTER_X, CENTER_Y - 12, 170, STL_DEFAULT, FNT_DEFAULT, n ? tail : "_");
        letter(CENTER_X, CENTER_Y + 18, 170, STL_GRAY, FNT_DEFAULT, "A add  B erase");
        letter(CENTER_X, CENTER_Y + 40, 170, STL_GRAY, FNT_DEFAULT, digits ? "C\u25B2\u25BC A-Z  START ok" : "C\u25B2\u25BC 0-9  START ok");
        return;
    }

    /* In the middle: what is spelled so far, with the letter pointed at. */
    char typed[PREFIX_MAX + 2];
    snprintf(typed, sizeof(typed), "%s%c", prefix, has_games[pointed] ? slot_char(pointed) : ' ');
    letter(CENTER_X, CENTER_Y - 14, 170, STL_DEFAULT, (strlen(typed) > 8) ? FNT_DEFAULT : FNT_TITLE, typed);
    letter(CENTER_X, CENTER_Y + 22, 170, STL_GRAY, FNT_DEFAULT, prefix[0] ? "A more  B back" : "A add letter");
}
