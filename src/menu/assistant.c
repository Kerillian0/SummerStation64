#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <libdragon.h>

#include "animations.h"
#include "assistant.h"
#include "fonts.h"
#include "games_ui.h"
#include "intro.h"
#include "menu_features.h"
#include "menu_options.h"
#include "cover_list.h"
#include "display_name.h"
#include "game_data.h"
#include "game_index.h"
#include "my_collections.h"
#include "play_stats.h"
#include "profiles.h"
#include "safe_file.h"
#include "theme.h"

#define FILE_NAME       "assistant.txt"     /* per player: profiles_file() */
#define FILE_TMP_NAME   "assistant.tmp"
#define FILE_HEADER     "assistant 1"

#define FAIRY_PATH      "rom:/fairy.sprite"
#define SPARKLE_PATH    "rom:/sparkle.sprite"     /* a 16 px star, tinted (make_fairy.py) */
#define FAIRY_SIZE      (40)        /* each of its two pictures (wings up, wings down) */
#define FAIRY_NAME      "Vee"
#define FAIRY_COLOR     RGBA32(0xFF, 0x7A, 0x6A, 0xFF)  /* its own coral: the edge of its text box */
#define SWAP_MS         (900)       /* Showy: a new line sweeping in while it stays (420 was too quick to see) */
#define SPARKLES        (10)
#define FLAP_MS         (110)

#define FIRST_TALK_MS   (45000)     /* not in the first moments after switching on */
#define QUIET_MS        (4000)      /* only once the player has stopped pressing things for this long */
#define ARRIVE_MS       (500)
#define LEAVE_MS        (350)
#define SAY_MS          (7000)      /* how long the bubble stays, unless a button sends it off sooner */

/* Where it shows (the owner's layout, 2026-11-11): on the cover rows the
   fairy hovers in the top right corner, in the Memory Badge's place under
   the clock, drawn just large enough to fill the gap above the covers
   (the right cover's top is at y 128, less its 4 px float); its words take
   the band of button hints at the bottom while it talks. On Game info,
   where the cover fills that corner, it sits at the band's right end. */
#define PERCH_SIZE      (42)        /* drawn this size */
#define PERCH_X         (GAMES_UI_CONTENT_X1 + 22) /* its middle: right of the title panel and position bar (they end at 560) */
#define PERCH_ABOVE     (8)         /* its middle this far above the band's top edge: perched on its corner */
#define BAND_PAD        (12)
#define TEXT_LENGTH     (200)

/* ---------- what it says ---------- */

typedef enum { WHERE_COVERS, WHERE_GAME_INFO } where_t;

typedef struct {
    where_t where;
    const char *eager;
    const char *snarky;
    bool (*done) (menu_t *menu);    /* the player already does this: don't suggest it */
} tip_t;

static uint32_t used_bits = 0;      /* assistant_used_t */
static uint32_t seen_bits = 0;      /* tips said, by their place in `tips` */
static uint32_t said_bits = 0;      /* milestones said, once ever (SAID_...) */

static bool used (assistant_used_t what) {
    return (used_bits & (1u << what)) != 0;
}

static bool done_letter_dial (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_LETTER_DIAL); }
static bool done_filter (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_FILTER); }
static bool done_screenshot (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_SCREENSHOT); }
static bool done_random (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_RANDOM); }
static bool done_details (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_DETAILS); }
static bool done_theme (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_THEME_EDITOR); }
static bool done_inspect (menu_t *menu) { (void) menu; return used(ASSISTANT_USED_INSPECT) || !features_enabled(FEATURE_BOX_3D); }
static bool done_showy (menu_t *menu) { (void) menu; return options_get(OPTION_ANIMATIONS) != 0; }
static bool done_collections (menu_t *menu) { (void) menu; return my_collections_count() > 0; }
static bool done_profiles (menu_t *menu) { (void) menu; return profiles_count() > 1; }
static bool done_never (menu_t *menu) { (void) menu; return false; }

static bool done_favorites (menu_t *menu) {
    for (int i = 0; i < FAVORITES_COUNT; i++) {
        if (menu->bookkeeping.favorite_items[i].primary_path) {
            return true;
        }
    }
    return false;
}

/* At most 32 (a bit each in assistant.txt); new ones go at the end, so
   what a player has seen keeps its place. */
static const tip_t tips[] = {
    { WHERE_COVERS, "Looking for a game? Hold Z and point the stick at a letter to jump straight there!",
                    "Still scrolling one by one? Hold Z and point at a letter. You're welcome.", done_letter_dial },
    { WHERE_COVERS, "C-Up and C-Down filter the covers: USA, Japan, Europe, 64DD, hacks and more!",
                    "C-Up and C-Down filter the covers. Revolutionary, I know.", done_filter },
    { WHERE_COVERS, "Love a game? Press Z on its cover and add it to your Favorites!",
                    "Z on a cover, Add to Favorites. Or go hunting for it every single time. Your call.", done_favorites },
    { WHERE_COVERS, "Can't decide what to play? Press Z, then Random game!",
                    "Can't choose? Z, Random game. Let fate decide, since you won't.", done_random },
    { WHERE_COVERS, "You can make your own collections! Z on a game, then Add to collection.",
                    "Z, Add to collection. Organize your life a little.", done_collections },
    { WHERE_COVERS, "Hold B on the box and move the stick to turn it all the way round!",
                    "Hold B and use the stick. Yes, the box spins. Try to contain yourself.", done_inspect },
    { WHERE_COVERS, "Want more sparkle? Settings > Look > Theme > Animations: try Showy!",
                    "This menu can dance, you know. Settings > Look > Theme > Animations > Showy.", done_showy },
    { WHERE_COVERS, "Hold START for a second to take a screenshot. They go in menu/screenshots!",
                    "Hold START a second for a screenshot. Proof, for when nobody believes you.", done_screenshot },
    { WHERE_GAME_INFO, "Press START here to see all of this game's details!",
                       "START shows the details. All the nerdy stuff. You'll love it.", done_details },
    { WHERE_COVERS, "Make the menu your own! Settings > Look > Theme > Edit Theme.",
                    "Bored of these colors? Settings > Look > Theme > Edit Theme. Go wild.", done_theme },
    { WHERE_COVERS, "Sharing this console? Settings > Games > Profiles gives every player their own saves!",
                    "Someone else playing on your saves? Settings > Games > Profiles. Fix that.", done_profiles },
    { WHERE_COVERS, "Missing some box art? The Companion web page can fill it in for you!",
                    "No box art? The Companion web page does it for you. Lazy, but smart.", done_never },
};
#define TIPS_COUNT  ((int) (sizeof(tips) / sizeof(tips[0])))

/* ---------- skins: other fairies (assets/assistant/README.md) ---------- */

/* Baked into the menu file by assets/assistant/make_skins.py (the owner's
   own builds only, never a release): rom:/assistant/list.txt names them, <name>.sprite is the
   picture (56 x 56, one, turned now and then), <name>.txt its name, color,
   home games (first three letters of their codes) and lines about them. */
#define SKINS_MAX       (8)
#define SKIN_SIZE       (56)
#define SKIN_LINES_MAX  (72)
#define SKIN_LINE_LENGTH (120)
#define TURN_EVERY_MS   (1400)      /* a skin faces one way this long, then turns to face the other */
#define TURN_MS         (220)

static char skin_files[SKINS_MAX][20];
static char skin_names[SKINS_MAX][24];
static int skin_count = -1;     /* -1: list not read yet */

static void skins_list (void) {
    if (skin_count >= 0) {
        return;
    }
    skin_count = 0;
    FILE *f = fopen("rom:/assistant/list.txt", "r");
    if (!f) {
        return;
    }
    char line[64];
    while (skin_count < SKINS_MAX && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) {
            continue;
        }
        snprintf(skin_files[skin_count], sizeof(skin_files[0]), "%s", line);
        snprintf(skin_names[skin_count], sizeof(skin_names[0]), "%s", line);
        char path[64];
        snprintf(path, sizeof(path), "rom:/assistant/%s.txt", line);
        FILE *t = fopen(path, "r");
        if (t) {
            char row[160];
            while (fgets(row, sizeof(row), t)) {
                if (strncmp(row, "name ", 5) == 0) {
                    row[strcspn(row, "\r\n")] = '\0';
                    snprintf(skin_names[skin_count], sizeof(skin_names[0]), "%s", row + 5);
                    break;
                }
            }
            fclose(t);
        }
        skin_count++;
    }
    fclose(f);
}

static const char *fairy_key (int choice) {
    return (choice >= 1 && choice <= skin_count) ? skin_files[choice - 1] : "own";
}

/* The player's own name for a fairy, or NULL. */
static const char *given_name (int choice);

const char *assistant_skin_name (int choice) {
    skins_list();
    if (choice >= 0 && choice <= skin_count) {
        const char *given = given_name(choice);
        if (given) {
            return given;
        }
    }
    if (choice == 0) {
        return FAIRY_NAME;
    }
    skins_list();
    return (choice >= 1 && choice <= skin_count) ? skin_names[choice - 1] : "";
}

/* The skin in use: Vee (choice 0) or one read from its file. */
static struct {
    int choice;                 /* -1: not loaded yet */
    char name[24];
    color_t color;
    char games[8][3];
    int game_count;
    struct {
        uint8_t kind;           /* LINE_... */
        char mood;              /* 'e' or 's' */
        char key[6];            /* LINE_ON: a code's first three letters, or "zelda" */
        char text[SKIN_LINE_LENGTH];
    } lines[SKIN_LINES_MAX];
    int line_count;
} skin = { .choice = -1 };

/* What a skin's line is for (skin.txt: "e"/"s" lines are its own games'). */
enum { LINE_HOME, LINE_ON, LINE_OPEN, LINE_BACK, LINE_KINDS };
static int line_next[LINE_KINDS];

static void skin_add (int kind, char mood, const char *key, const char *text) {
    if (skin.line_count >= SKIN_LINES_MAX || (mood != 'e' && mood != 's') || !text[0]) {
        return;
    }
    skin.lines[skin.line_count].kind = (uint8_t) kind;
    skin.lines[skin.line_count].mood = mood;
    snprintf(skin.lines[skin.line_count].key, sizeof(skin.lines[0].key), "%s", key);
    snprintf(skin.lines[skin.line_count].text, SKIN_LINE_LENGTH, "%s", text);
    skin.line_count++;
}

static void skin_load (void) {
    int choice = options_get(OPTION_ASSISTANT_SKIN);
    skins_list();
    if (choice < 0 || choice > skin_count) {
        choice = 0;     /* a skin no longer in the menu file */
    }
    if (choice == skin.choice) {
        return;
    }
    memset(&skin, 0, sizeof(skin));
    skin.choice = choice;
    snprintf(skin.name, sizeof(skin.name), "%s", FAIRY_NAME);
    skin.color = FAIRY_COLOR;
    if (choice == 0) {
        return;
    }
    snprintf(skin.name, sizeof(skin.name), "%s", skin_names[choice - 1]);
    char path[64];
    snprintf(path, sizeof(path), "rom:/assistant/%s.txt", skin_files[choice - 1]);
    FILE *f = fopen(path, "r");
    if (!f) {
        return;
    }
    char row[SKIN_LINE_LENGTH + 80];    /* room for "on zelda e " and the like before the line */
    while (fgets(row, sizeof(row), f)) {
        row[strcspn(row, "\r\n")] = '\0';
        unsigned int rgb;
        if (sscanf(row, "color %x", &rgb) == 1) {
            skin.color = RGBA32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
        } else if (strncmp(row, "games ", 6) == 0) {
            for (char *code = strtok(row + 6, " "); code && skin.game_count < 8; code = strtok(NULL, " ")) {
                if (strlen(code) >= 3) {
                    memcpy(skin.games[skin.game_count++], code, 3);
                }
            }
        } else if ((row[0] == 'e' || row[0] == 's') && row[1] == ' ') {
            skin_add(LINE_HOME, row[0], "", row + 2);         /* about its own games */
        } else if (strncmp(row, "open ", 5) == 0 && row[6] == ' ') {
            skin_add(LINE_OPEN, row[5], "", row + 7);         /* opening one of its games' info */
        } else if (strncmp(row, "back ", 5) == 0 && row[6] == ' ') {
            skin_add(LINE_BACK, row[5], "", row + 7);         /* back from playing one */
        } else if (strncmp(row, "on ", 3) == 0) {
            char key[8], mood[2];
            int used = 0;
            if (sscanf(row + 3, "%7s %1s %n", key, mood, &used) == 2 && used > 0) {
                skin_add(LINE_ON, mood[0], key, row + 3 + used);   /* about another game */
            }
        }
    }
    fclose(f);
    debugf("assistant: skin %s, %d games, %d lines\n", skin.name, skin.game_count, skin.line_count);
}

/* Is this game one of the skin's own (by the first three letters of its code)? */
static bool skin_home (const char *code) {
    for (int i = 0; code && code[0] && i < skin.game_count; i++) {
        if (memcmp(skin.games[i], code, 3) == 0) {
            return true;
        }
    }
    return false;
}

/* ---------- the player's file ---------- */

/* Names the player gave their fairies (per player, in assistant.txt as
   "name <fairy> <name>"; the menu's own fairy is "own", a skin its folder). */
#define NAMES_MAX       (SKINS_MAX + 1)
static struct { char key[20]; char name[ASSISTANT_NAME_MAX + 1]; } names[NAMES_MAX];
static int name_count = 0;

static int loaded_for = -1;     /* profiles_generation() */
static bool dirty = false;

static void load (void) {
    if (loaded_for == profiles_generation()) {
        return;
    }
    loaded_for = profiles_generation();
    used_bits = seen_bits = said_bits = 0;
    name_count = 0;
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", profiles_file(FILE_TMP_NAME));
    FILE *f = fopen(profiles_file(FILE_NAME), "r");
    if (!f) {
        f = fopen(tmp, "r");    /* a save that was cut off */
    }
    if (!f) {
        return;
    }
    char line[64];
    if (fgets(line, sizeof(line), f) && strncmp(line, FILE_HEADER, strlen(FILE_HEADER)) == 0) {
        while (fgets(line, sizeof(line), f)) {
            unsigned long bits;
            if (sscanf(line, "seen %lx", &bits) == 1) {
                seen_bits = (uint32_t) bits;
            } else if (sscanf(line, "used %lx", &bits) == 1) {
                used_bits |= (uint32_t) bits;
            } else if (sscanf(line, "said %lx", &bits) == 1) {
                said_bits = (uint32_t) bits;
            } else if (strncmp(line, "name ", 5) == 0 && name_count < NAMES_MAX) {
                char *key = line + 5;
                char *text = strchr(key, ' ');
                if (text) {
                    *text++ = '\0';
                    text[strcspn(text, "\r\n")] = '\0';
                    snprintf(names[name_count].key, sizeof(names[0].key), "%s", key);
                    snprintf(names[name_count].name, sizeof(names[0].name), "%s", text);
                    name_count += (text[0] != '\0');
                }
            }
        }
    }
    fclose(f);
}

static void save (void) {
    char path[256], tmp[256];
    snprintf(path, sizeof(path), "%s", profiles_file(FILE_NAME));
    snprintf(tmp, sizeof(tmp), "%s", profiles_file(FILE_TMP_NAME));
    FILE *f = fopen(tmp, "w");
    if (!f) {
        debugf("assistant: could not write %s\n", tmp);
        return;
    }
    fprintf(f, "%s\nseen %08lx\nused %08lx\nsaid %08lx\n", FILE_HEADER,
            (unsigned long) seen_bits, (unsigned long) used_bits, (unsigned long) said_bits);
    for (int i = 0; i < name_count; i++) {
        fprintf(f, "name %s %s\n", names[i].key, names[i].name);
    }
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(tmp, path)) {
        debugf("assistant: could not replace %s\n", path);
        return;
    }
    dirty = false;
}

static const char *given_name (int choice) {
    load();
    const char *key = fairy_key(choice);
    for (int i = 0; i < name_count; i++) {
        if (strcmp(names[i].key, key) == 0) {
            return names[i].name;
        }
    }
    return NULL;
}

/* What the fairy in use is called: the player's name for it, or its own. */
static const char *fairy_name (void) {
    skin_load();
    const char *given = given_name(skin.choice);
    return given ? given : skin.name;
}

void assistant_rename (const char *name) {
    skin_load();
    load();
    const char *key = fairy_key(skin.choice);
    int at = 0;
    while (at < name_count && strcmp(names[at].key, key) != 0) {
        at++;
    }
    if (!name || !name[0]) {
        if (at < name_count) {
            names[at] = names[--name_count];    /* back to its usual name */
        }
    } else {
        if (at == name_count) {
            if (name_count >= NAMES_MAX) {
                return;
            }
            name_count++;
        }
        snprintf(names[at].key, sizeof(names[0].key), "%s", key);
        snprintf(names[at].name, sizeof(names[0].name), "%s", name);
    }
    save();     /* a name is worth keeping at once */
}

void assistant_note_used (assistant_used_t what) {
    load();
    if (!used(what)) {
        used_bits |= 1u << what;
        dirty = true;   /* saved with the next thing said */
    }
}

/* ---------- talking ---------- */

typedef enum { QUIET, ARRIVING, TALKING, LEAVING } state_t;

static state_t state = QUIET;
static uint64_t state_since = 0;
static uint64_t last_talk_ms = 0;
static int talk_screen = -1;                /* the screen it is talking on */
static rdpq_paragraph_t *words = NULL;     /* what it is saying, laid out once */
static int words_height = 0;
static rdpq_paragraph_t *old_words = NULL; /* Showy: the line before, sweeping away */
static int old_height = 0;
static uint64_t swap_since = 0;
static sprite_t *fairy = NULL;
static sprite_t *sparkle = NULL;
static int next_tip = 0;                    /* where to start looking, so tips take turns */
static int saying_tip = -1;                 /* the tip being said: counted as said once it is talking */
static uint32_t saying_bits = 0;            /* the milestones being said: the same */

void assistant_reset (void) {
    load();
    seen_bits = 0;
    save();
    last_talk_ms = 0;
    next_tip = 0;
    debugf("assistant: tips start over\n");
}

const char *assistant_often_name (int choice) {
    static const char *names[ASSISTANT_COUNT] = { "Off", "Rarely", "Now and Then", "Chatty" };
    return (choice >= 0 && choice < ASSISTANT_COUNT) ? names[choice] : "";
}

const char *assistant_mood_name (int choice) {
    static const char *names[ASSISTANT_MOOD_COUNT] = { "Eager", "Snarky" };
    return (choice >= 0 && choice < ASSISTANT_MOOD_COUNT) ? names[choice] : "";
}

/* The fairy's colour (Settings > Look > Fairy Look > Fairy Color): its own,
   the theme's accent, or one of these. */
static const struct { const char *name; uint32_t rgb; } fairy_colors[] = {
    { "Coral", 0xFF7A6A }, { "Gold", 0xFFC83C }, { "Lime", 0x9BE04A }, { "Mint", 0x4CE0B0 },
    { "Sky", 0x5CC8FF }, { "Blue", 0x4A78FF }, { "Violet", 0xA070FF }, { "Pink", 0xFF6EC8 },
    { "Red", 0xFF4A4A }, { "White", 0xF4F4F4 },
};
#define FAIRY_COLORS    ((int) (sizeof(fairy_colors) / sizeof(fairy_colors[0])))

const char *assistant_color_name (int choice) {
    if (choice == 0) return "Its Own";
    if (choice == 1) return "Theme Accent";
    return (choice >= 2 && choice < 2 + FAIRY_COLORS) ? fairy_colors[choice - 2].name : "";
}

static color_t fairy_tint (void) {
    int choice = options_get(OPTION_ASSISTANT_COLOR);
    if (choice == 1) {
        return theme_get()->accent;
    }
    if (choice >= 2 && choice < 2 + FAIRY_COLORS) {
        uint32_t rgb = fairy_colors[choice - 2].rgb;
        return RGBA32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
    }
    return skin.color;
}

static uint64_t gap_ms (int often) {
    switch (often) {
        case ASSISTANT_RARELY: return 12 * 60 * 1000;
        case ASSISTANT_CHATTY: return 2 * 60 * 1000;
        default: return 5 * 60 * 1000;
    }
}

static bool screen_is (menu_t *menu, where_t where) {
    if (where == WHERE_GAME_INFO) {
        return menu->mode == MENU_MODE_LOAD_ROM;
    }
    if (menu->mode == MENU_MODE_BROWSER) {
        return games_ui_tab_shown() != GAMES_TAB_FOLDERS;  /* its file list starts right under the tab bar, in the fairy's corner */
    }
    return menu->mode == MENU_MODE_HISTORY || menu->mode == MENU_MODE_FAVORITE;
}

static void let_go (void) {
    if (words) {
        rdpq_paragraph_free(words);
        words = NULL;
    }
    if (old_words) {
        rdpq_paragraph_free(old_words);
        old_words = NULL;
    }
    if (fairy) {
        sprite_free(fairy);
        fairy = NULL;
    }
    if (sparkle) {
        sprite_free(sparkle);
        sparkle = NULL;
    }
    state = QUIET;
}


/* The text's width in the band. */
static int band_text_width (void) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    return (x1 - x0) - (BAND_PAD * 2);
}

/* Lay out what it says, in the band's width: its name in orange first. */
static void set_words (const char *message, bool swap) {
    if (old_words) {
        rdpq_paragraph_free(old_words);
        old_words = NULL;
    }
    if (words && swap && animations_showy()) {
        old_words = words;      /* swept away as the new line comes in */
        old_height = words_height;
        swap_since = get_ticks_ms();
    } else if (words) {
        rdpq_paragraph_free(words);
    }
    /* Its name in orange first, then what it says. */
    char text[TEXT_LENGTH + 32];
    snprintf(text, sizeof(text), "^%02X%s:^00 %s", STL_FAIRY, fairy_name(), message);   /* the name in its colour */
    int bytes = strlen(text);
    words = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .width = band_text_width(),
        .height = 200,
        .wrap = WRAP_WORD,
        .style_id = STL_DEFAULT,
    }, FNT_DEFAULT, text, &bytes);
    words_height = (int) (words->bbox.y1 - words->bbox.y0);
}

static void say (const char *message) {
    skin_load();
    if (!fairy) {
        if (skin.choice > 0) {
            char path[64];
            snprintf(path, sizeof(path), "rom:/assistant/%s.sprite", skin_files[skin.choice - 1]);
            fairy = sprite_load(path);
        } else {
            fairy = sprite_load(FAIRY_PATH);
        }
        sparkle = sprite_load(SPARKLE_PATH);
    }
    set_words(message, false);
    state = ARRIVING;
    state_since = get_ticks_ms();
}

/* ---------- reactions and milestones ---------- */

/* Milestones, said once ever (`said_bits`). */
#define SAID_HOURS      (0)     /* 0-5: 10, 25, 50, 100, 250, 500 hours played */
#define SAID_GAMES      (6)     /* 6-9: 10, 25, 50, 100 different games */
#define SAID_64DD       (10)    /* the first 64DD disk played */
static const int hour_marks[] = { 10, 25, 50, 100, 250, 500 };
static const int game_marks[] = { 10, 25, 50, 100 };

#define EARLY_MS        (10000)     /* a reaction waits at least this long after switching on */
#define REACT_QUIET_MS  (2500)
#define REACT_GAP_MS    (30000)     /* and this long after anything else said */
#define LONG_SESSION    (20 * 60)   /* a session this long is worth a word on the way back */
#define VERY_LONG       (2 * 60 * 60)
#define UNPLAYED_DAYS   (365)

#define PENDING_MAX     (4)
static struct {
    char text[TEXT_LENGTH];
    uint32_t bits;          /* milestones it says (marked once it is talking) */
    uint64_t not_before;    /* 0: a welcome, said first; else not before this time since power-on */
} pending[PENDING_MAX];
static int pending_count = 0;
static bool started_checked = false;

static bool snarky (void) {
    return options_get(OPTION_ASSISTANT_MOOD) == ASSISTANT_SNARKY;
}

static uint64_t queue_not_before = 0;  /* what queue() gives the next ones */

static void queue (uint32_t bits, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void queue (uint32_t bits, const char *fmt, ...) {
    if (pending_count >= PENDING_MAX) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(pending[pending_count].text, TEXT_LENGTH, fmt, args);
    va_end(args);
    pending[pending_count].bits = bits;
    pending[pending_count].not_before = queue_not_before;
    pending_count++;
}

/* "45 minutes", "1 hour 10 minutes", "3 hours". */
static const char *duration (uint32_t seconds, char *out, size_t size) {
    int hours = (int) (seconds / 3600), minutes = (int) ((seconds % 3600) / 60);
    if (hours == 0) {
        snprintf(out, size, "%d minute%s", minutes, (minutes == 1) ? "" : "s");
    } else if (minutes == 0 || hours >= 10) {
        snprintf(out, size, "%d hour%s", hours, (hours == 1) ? "" : "s");
    } else {
        snprintf(out, size, "%d hour%s %d minute%s", hours, (hours == 1) ? "" : "s", minutes, (minutes == 1) ? "" : "s");
    }
    return out;
}

static const char *ordinal (int n) {
    int tens = n % 100;
    if (tens >= 11 && tens <= 13) return "th";
    switch (n % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

/* A game's tidy name from its full path. */
static const char *game_name (const char *path, char *out, size_t size) {
    const char *slash = strrchr(path, '/');
    snprintf(out, size, "%s", display_name_file(slash ? slash + 1 : path));
    return out;
}

/* The highest mark reached that hasn't been said: said, with the ones
   below it marked as said too (a player with 300 hours hears about 250,
   not about 10, 25, 50 and 100 as well). -1 if none. */
static int new_mark (const int *marks, int count, int first_bit, int value, uint32_t *bits) {
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (value >= marks[i] && !(said_bits & (1u << (first_bit + i)))) {
            found = i;
        }
    }
    *bits = 0;
    if (found >= 0) {
        for (int i = 0; i <= found; i++) {
            *bits |= 1u << (first_bit + i);
        }
    }
    return found;
}

/* Once, when the covers first show: back from a game, a milestone, the hour. */
static bool skin_pick (int kind, const char *key, char *out, size_t size);   /* further down, with the game lines */

/* Said at power-on: the day first (a holiday, the N64's birthday), else
   the weekend, else the time of day. */
static void greeting (menu_t *menu, bool welcomed_back) {
    if (menu->current_time <= 0) {
        return;
    }
    struct tm *now = localtime(&menu->current_time);
    if (!now) {
        return;
    }
    int month = now->tm_mon + 1, day = now->tm_mday, hour = now->tm_hour;
    bool s = snarky();
    if (month == 1 && day == 1) {
        queue(0, "%s", s ? "New year, same old backlog." : "Happy New Year! Here's to a year full of games!");
    } else if (month == 2 && day == 14) {
        queue(0, "%s", s ? "Valentine's Day. Player two, where are you?" : "Happy Valentine's Day! Share a two-player game with someone special!");
    } else if (month == 6 && day == 23) {
        int years = now->tm_year + 1900 - 1996;     /* it came out in Japan on 23 June 1996 */
        if (s) {
            queue(0, "The N64 turns %d today. Still going strong. Unlike its first Rumble Pak batteries.", years);
        } else {
            queue(0, "Happy birthday, Nintendo 64! %d years old today!", years);
        }
    } else if (month == 10 && day == 31) {
        queue(0, "%s", s ? "Halloween. Turn off the lights and pick something scary. I dare you." : "Happy Halloween! Something spooky, perhaps?");
    } else if (month == 12 && day == 24) {
        queue(0, "%s", s ? "Christmas Eve. Still hoping for a new cartridge under the tree?" : "It's Christmas Eve! Time to cozy up with a favorite!");
    } else if (month == 12 && day == 25) {
        queue(0, "%s", s ? "Merry Christmas. Now, about that backlog." : "Merry Christmas! I hope you got some new games!");
    } else if (welcomed_back) {
        /* the welcome back is the greeting */
    } else if (hour < 5) {
        queue(0, "%s", s ? "It's past midnight. The games will still be here tomorrow. Probably."
                         : "It's getting late! One more game, then off to bed?");
    } else if (now->tm_wday == 0 || now->tm_wday == 6) {
        queue(0, "%s", s ? "The weekend. No excuses now." : "It's the weekend! Plenty of time for games!");
    } else if (hour < 12) {
        queue(0, "%s", s ? "Morning already? Fine. Pick a game." : "Good morning! Ready for some games?");
    } else if (hour < 17) {
        queue(0, "%s", s ? "Afternoon gaming. Very productive." : "Good afternoon! Perfect time for a game!");
    } else if (hour < 22) {
        queue(0, "%s", s ? "Evening. The best time to ignore everything else." : "Good evening! Time to unwind with a game!");
    } else {
        queue(0, "%s", s ? "Late again? Sleep is overrated, apparently." : "It's getting late! Maybe one game before bed?");
    }
}

/* Back from a game, after a minute or more. */
static const char *back_eager[] = { "Welcome back from %s! Was it fun?", "Back from %s! How did it go?" };
static const char *back_snarky[] = { "Back from %s already? That was quick.", "%s again, huh. Did you win this time?" };
static int back_next = 0;

static void started_check (menu_t *menu) {
    char name[96], took[48], total[48], text[TEXT_LENGTH];
    const char *rom;
    uint32_t length, count, seconds;
    skin_load();
    queue_not_before = 0;               /* welcomes: said first */
    if (play_stats_settled(&rom, &length, &count, &seconds)) {
        game_name(rom, name, sizeof(name));
        const char *dot = strrchr(rom, '.');
        const game_index_entry_t *known = game_index_find(rom, 0);
        char code[5] = "";
        if (known) {
            memcpy(code, known->code, 4);
            code[4] = '\0';
        }
        if (dot && strcasecmp(dot, ".ndd") == 0 && !(said_bits & (1u << SAID_64DD))) {
            queue(1u << SAID_64DD, "%s", snarky()
                ? "A 64DD game. Look at you, collecting the rare stuff."
                : "Your first 64DD game! Not many people have ever seen one of those run!");
        } else if (count == 10 || count == 25 || count == 50 || count == 100 || count == 250) {
            if (snarky()) {
                queue(0, "%s, for the %d%s time. Ever heard of other games?", name, (int) count, ordinal((int) count));
            } else {
                queue(0, "That was your %d%s time playing %s! You really love that one!", (int) count, ordinal((int) count), name);
            }
        } else if (length >= 60 && skin_home(code) && skin_pick(LINE_BACK, NULL, text, sizeof(text))) {
            queue(0, "%s", text);           /* back from one of the skin's own games */
        } else if (length >= VERY_LONG) {
            duration(length, took, sizeof(took));
            if (snarky()) {
                queue(0, "%s of %s. Your thumbs called. They want a lawyer.", took, name);
            } else {
                queue(0, "Wow, %s of %s! Remember to stretch those thumbs!", took, name);
            }
        } else if (length >= LONG_SESSION) {
            duration(length, took, sizeof(took));
            duration(seconds, total, sizeof(total));
            if (snarky()) {
                queue(0, "Back from %s after %s? I was starting to think you'd moved in.", name, took);
            } else {
                queue(0, "Welcome back! %s for %s. That makes %s in all!", name, took, total);
            }
        } else if (length >= 60) {
            if (skin_home(code) && skin_pick(LINE_BACK, NULL, text, sizeof(text))) {
                queue(0, "%s", text);       /* back from one of the skin's own games */
            } else {
                queue(0, snarky() ? back_snarky[back_next % 2] : back_eager[back_next % 2], name);
                back_next++;
            }
        }
    }
    if (intro_power_on()) {
        greeting(menu, pending_count > 0);
    }

    /* Milestones: later, never as the first thing said. */
    queue_not_before = FIRST_TALK_MS;
    uint32_t all_seconds;
    int games;
    play_stats_totals(&all_seconds, &games);
    uint32_t bits;
    int mark = new_mark(hour_marks, (int) (sizeof(hour_marks) / sizeof(hour_marks[0])), SAID_HOURS, (int) (all_seconds / 3600), &bits);
    if (mark >= 0) {
        if (snarky()) {
            queue(bits, "%d hours of games on this menu. I'm not judging. Much.", hour_marks[mark]);
        } else {
            queue(bits, "You've played %d hours of games on this menu! Here's to %d more!", hour_marks[mark], hour_marks[mark]);
        }
    }
    mark = new_mark(game_marks, (int) (sizeof(game_marks) / sizeof(game_marks[0])), SAID_GAMES, games, &bits);
    if (mark >= 0) {
        if (snarky()) {
            queue(bits, "%d different games tried. Commitment issues?", game_marks[mark]);
        } else {
            queue(bits, "That's %d different games you've played! What a collection!", game_marks[mark]);
        }
    }
}

/* Resting on a game not played in over a year: once per game while the console is on. */
#define UNPLAYED_KEPT   (16)
static uint32_t unplayed_said[UNPLAYED_KEPT];
static int unplayed_next = 0;

static bool unplayed_check (menu_t *menu) {
    if (menu->current_time <= 0) {
        return false;
    }
    const cover_list_t *covers = cover_list_current(menu);
    if (covers->entries <= 0 || covers->selected < 0 || covers->selected >= covers->entries) {
        return false;
    }
    path_t *path = covers->path(menu, covers->selected);
    time_t last = 0;
    int count = play_stats_get(path_get(path), &last);
    uint32_t key = 2166136261u;
    for (const unsigned char *c = (const unsigned char *) path_get(path); *c; c++) {
        key = (key ^ *c) * 16777619u;
    }
    bool old = count > 0 && last > 0 && (menu->current_time - last) > (time_t) UNPLAYED_DAYS * 24 * 60 * 60;
    for (int i = 0; i < UNPLAYED_KEPT && old; i++) {
        if (unplayed_said[i] == key) {
            old = false;
        }
    }
    if (old) {
        char name[96];
        game_name(path_get(path), name, sizeof(name));
        unplayed_said[unplayed_next] = key;
        unplayed_next = (unplayed_next + 1) % UNPLAYED_KEPT;
        if (snarky()) {
            queue(0, "%s hasn't been touched in over a year. It misses you. Or not.", name);
        } else {
            queue(0, "You haven't played %s in over a year! Maybe it's time?", name);
        }
    }
    path_free(path);
    return old;
}

/* ---------- staying around while the player is idle ---------- */

/* After a minute without a press it comes and stays, a new line every few
   seconds, until a button is pressed: about the game it is resting on, a
   joke, a look at the play stats, or a tip (said before or not). */
#define LINGER_AFTER_MS (60000)
#define HOME_QUIET_MS   (2500)      /* resting on the skin's own game this long */
#define HOME_GAP_MS     (45000)     /* and this long after anything else said (Now and Then; Rarely 3 min) */
#define CHATTY_REST_MS  (5000)      /* Chatty: resting this long on any game gets a word about it, no gap */
#define LINGER_SAY_MS   (8000)

static bool lingering = false;
static int linger_step = 0;
static int joke_next = 0, game_next = 0, stats_next = 0, any_tip_next = 0;

static const char *jokes_eager[] = {
    "Fun fact: blowing on cartridges never really helped. It sure felt like it did, though!",
    "The controller has three handles. Has anyone ever needed all three at once?",
    "The Expansion Pak adds four whole megabytes. That's a lot of joy in one little pak!",
    "Remember to take a break now and then. Your games will wait for you!",
    "I love the click a cartridge makes when it goes in. So satisfying!",
    "Four controller ports! This console was made for friends on the couch.",
    "Rumble Paks ran on batteries. Feeling every crash took real dedication!",
    "Every game here is a little time machine. Where shall we go today?",
};
static const char *jokes_snarky[] = {
    "Blowing on cartridges did nothing, you know. Science says so. Childhood disagrees.",
    "Three handles on that controller, and you still only ever hold two.",
    "Still here? These games aren't going to play themselves.",
    "The Controller Pak: 123 pages of memory. Pure luxury.",
    "Somewhere, an old Rumble Pak battery is quietly leaking. Just saying.",
    "I'd pick a game for you, but I have no hands. Or taste.",
    "You've been staring at this menu so long, I'm starting to blush.",
    "The cartridge tilt trick: an ancient art, passed down through the generations.",
};
#define JOKES_COUNT ((int) (sizeof(jokes_eager) / sizeof(jokes_eager[0])))

typedef struct {
    const char *genre;      /* as in rom:/gamedata.txt */
    const char *eager;      /* %s: the game's name */
    const char *snarky;
} genre_line_t;

static const genre_line_t genre_lines[] = {
    { "Sports", "%s! Time to get competitive!", "%s. Sweaty palms incoming." },
    { "Racing", "%s! Ready, set, go!", "%s. Try to stay on the track this time." },
    { "Shooter", "%s! Lock and load!", "%s. Aim for the targets, not the walls." },
    { "Platform", "%s! Lots of jumping ahead. Mind the gaps!", "%s. How many lives will it take this time?" },
    { "Action", "%s looks like an adventure waiting to happen!", "%s. Button mashing counts as skill, right?" },
    { "Fighting", "%s! Practice those combos!", "%s. Settle your arguments here, not on the couch." },
    { "Puzzle", "%s! A puzzle a day keeps the brain sharp!", "%s. Don't worry, I won't tell anyone you looked up the answer." },
    { "RPG", "%s! Grab a snack, this could be a long one!", "%s. Say goodbye to your weekend." },
    { "Board", "%s! Perfect for a game night with friends!", "%s. Friendships have ended over less." },
    { "Simulation", "%s! Build something amazing!", "%s. Real life not exciting enough?" },
    { "Strategy", "%s! Think three moves ahead!", "%s. Thinking: like gaming, but slower." },
    { "Adventure", "%s! Adventure awaits!", "%s. Off to save someone again, I suppose." },
};

/* The game the screen is on: its name, path and what the table knows. */
static char code_here[5];     /* game_here(): the game's code, "" if not known */

static bool game_here (menu_t *menu, char *name, size_t name_size, char *path_out, size_t path_size, const game_data_t **data) {
    *data = NULL;
    code_here[0] = '\0';
    if (menu->mode == MENU_MODE_LOAD_ROM) {
        if (!menu->load.rom_path) {
            return false;
        }
        snprintf(path_out, path_size, "%s", path_get(menu->load.rom_path));
        game_name(path_out, name, name_size);
        memcpy(code_here, menu->load.rom_info.game_code, 4);
        code_here[4] = '\0';
        *data = game_data_find(code_here);
        return true;
    }
    const cover_list_t *covers = cover_list_current(menu);
    if (covers->entries <= 0 || covers->selected < 0 || covers->selected >= covers->entries) {
        return false;
    }
    entry_t *entry = &covers->list[covers->selected];
    if (entry->type != ENTRY_TYPE_ROM && entry->type != ENTRY_TYPE_DISK && entry->type != ENTRY_TYPE_EMULATOR) {
        return false;
    }
    path_t *path = covers->path(menu, covers->selected);
    snprintf(path_out, path_size, "%s", path_get(path));
    path_free(path);
    snprintf(name, name_size, "%s", display_name(entry));
    const game_index_entry_t *known = game_index_find(path_out, (covers->list == menu->browser.list) ? entry->size : 0);
    if (known) {
        memcpy(code_here, known->code, 4);
        code_here[4] = '\0';
    }
    *data = known ? game_data_find(known->code) : NULL;
    return true;
}

/* One of the skin's lines of this kind (and key), in the mood chosen, in turn. */
static bool skin_pick (int kind, const char *key, char *out, size_t size) {
    char mood = snarky() ? 's' : 'e';
    int found[SKIN_LINES_MAX], n = 0;
    for (int i = 0; i < skin.line_count; i++) {
        if (skin.lines[i].kind == kind && skin.lines[i].mood == mood && (!key || strcmp(skin.lines[i].key, key) == 0)) {
            found[n++] = i;
        }
    }
    if (n == 0) {
        return false;
    }
    snprintf(out, size, "%s", skin.lines[found[line_next[kind]++ % n]].text);
    return true;
}

static bool home_line (char *out, size_t size) {
    return skin_pick(LINE_HOME, NULL, out, size);
}

/* Does `text` hold `word`, whatever the case? */
static bool holds (const char *text, const char *word) {
    size_t n = strlen(word);
    for (; *text; text++) {
        if (strncasecmp(text, word, n) == 0) {
            return true;
        }
    }
    return false;
}

/* Lines every fairy has for well-known games, found by the name shown (so
   every dump and region matches). The first match wins: the more particular
   names come first. Our own words, about the games, never quoted from them. */
typedef struct {
    const char *name;
    const char *eager;
    const char *snarky;
} game_line_t;

static const game_line_t game_lines[] = {
    { "Mario Kart", "Mario Kart 64! Watch out for those blue shells!", "Mario Kart 64. Friendships will be tested." },
    { "Mario Party", "Mario Party! Grab some friends and some dice!", "Mario Party. Say goodbye to your palms. And your friends." },
    { "Mario Tennis", "Mario Tennis! Game, set, match!", "Mario Tennis. Try not to serve into the net this time." },
    { "Mario Golf", "Mario Golf! Fore!", "Mario Golf. Mind the sand traps. And the water. And the trees." },
    { "Paper Mario", "Paper Mario! A flat-out great adventure!", "Paper Mario. Thin hero, thick story." },
    { "Super Mario 64", "Super Mario 64! Time to jump into some paintings!", "Super Mario 64. 120 stars. No pressure." },
    { "Smash", "Super Smash Bros.! May the best brawler win!", "Smash Bros. Friendships end over items in this one." },
    { "GoldenEye", "GoldenEye 007! Time for some secret agent work!", "GoldenEye. Whoever picks the short character owes everyone an apology." },
    { "Perfect Dark", "Perfect Dark! Joanna's ready for action!", "Perfect Dark. Very dark. Fairly perfect." },
    { "Banjo-Tooie", "Banjo-Tooie! The bear and the bird are back!", "Banjo-Tooie. More jiggies. More notes. More everything." },
    { "Banjo", "Banjo-Kazooie! Let's collect some jiggies!", "Banjo-Kazooie. Hope you like musical notes. All of them." },
    { "Donkey Kong 64", "Donkey Kong 64! So many bananas to find!", "Donkey Kong 64. Bring snacks. You'll be collecting for a while." },
    { "Diddy Kong", "Diddy Kong Racing! Cars, boats and planes!", "Diddy Kong Racing. Hope you like balloons." },
    { "Conker", "Conker's Bad Fur Day! Maybe not one for the kids...", "Conker's Bad Fur Day. Hide this one from your little siblings." },
    { "Star Fox", "Star Fox 64! Time to try a barrel roll!", "Star Fox 64. Someone will tell you to roll. Just do it." },
    { "F-Zero", "F-Zero X! Thirty racers at top speed!", "F-Zero X. Blink and you've crashed." },
    { "Wave Race", "Wave Race 64! Surf's up!", "Wave Race 64. The water's beautiful. Your landings, less so." },
    { "1080", "1080 Snowboarding! Hit the slopes!", "1080 Snowboarding. Try landing one of those spins." },
    { "Pokemon Snap", "Pokemon Snap! Say cheese!", "Pokemon Snap. The only game where throwing apples at wildlife is encouraged." },
    { "Pokemon Stadium", "Pokemon Stadium! Bring your team over from Game Boy!", "Pokemon Stadium. Hope your Transfer Pak still works." },
    { "Pokemon Puzzle", "Pokemon Puzzle League! Line them up!", "Pokemon Puzzle League. The blocks rise. So does the panic." },
    { "Yoshi", "Yoshi's Story! What a happy little world!", "Yoshi's Story. So much fruit. So little time." },
    { "Kirby", "Kirby 64! Mix up those powers!", "Kirby 64. Small, pink, eats everything. Relatable." },
    { "Turok", "Turok! Watch out for dinosaurs!", "Turok. The fog is there to keep things mysterious. Totally." },
    { "Blast Corps", "Blast Corps! Clear the way!", "Blast Corps. Knocking down buildings for a good cause." },
    { "Jet Force", "Jet Force Gemini! Let's save those Tribals!", "Jet Force Gemini. Count the Tribals. All of them." },
    { "Pilotwings", "Pilotwings 64! Up, up and away!", "Pilotwings 64. Gentle landings are optional, apparently." },
    { "Harvest Moon", "Harvest Moon 64! Time to tend the farm!", "Harvest Moon 64. The turnips won't water themselves." },
    { "Bomberman", "Bomberman 64! Blast away!", "Bomberman 64. Mind your own bombs." },
    { "Doom", "Doom 64! Lights down, volume up!", "Doom 64. Bring a flashlight. And courage." },
    { "Glover", "Glover! A magic glove and a bouncy ball!", "Glover. A glove. A ball. Physics." },
    { "Episode I", "Star Wars Racer! Start your engines!", "Star Wars Racer. Lean into the turns. Literally." },
    { "Rogue Squadron", "Rogue Squadron! Into the cockpit!", "Rogue Squadron. Hope you like fog. And lasers." },
    { "Tetris", "Tetris! Stack them up!", "Tetris. You'll see falling blocks when you close your eyes tonight." },
    { "Excitebike", "Excitebike 64! Pop a wheelie!", "Excitebike 64. Mind the jumps. And the landings." },
    { "Ogre Battle", "Ogre Battle 64! A grand campaign awaits!", "Ogre Battle 64. Clear your schedule." },
    { "Snowboard Kids", "Snowboard Kids! Hit the slopes!", "Snowboard Kids. Cute, and secretly ruthless." },
    { "Rayman", "Rayman 2! No arms, no legs, no problem!", "Rayman 2. The limbs still aren't attached." },
    { "Ocarina of Time", "Ocarina of Time! Hyrule awaits!", "Ocarina of Time. Fairy not included. Well, one is." },
    { "Majora", "Majora's Mask! Three days to save Termina!", "Majora's Mask. The moon is watching you. Again." },
};

/* Something particular about this game: the skin's own (its home games),
   its words about another game (by code, or "zelda" for the rest of that
   series), else the line every fairy has for a well-known game. */
static bool special_line (const char *code, const char *name, char *out, size_t size) {
    if (skin_home(code)) {
        return home_line(out, size);
    }
    char key[4] = "";
    if (code && strlen(code) >= 3) {
        memcpy(key, code, 3);
        key[3] = '\0';
        if (skin_pick(LINE_ON, key, out, size)) {
            return true;
        }
    }
    if (holds(name, "zelda") && skin_pick(LINE_ON, "zelda", out, size)) {
        return true;
    }
    for (int i = 0; i < (int) (sizeof(game_lines) / sizeof(game_lines[0])); i++) {
        if (holds(name, game_lines[i].name)) {
            snprintf(out, size, "%s", snarky() ? game_lines[i].snarky : game_lines[i].eager);
            return true;
        }
    }
    return false;
}

/* Something about the game it is resting on. */
static bool line_game (menu_t *menu, char *out, size_t size) {
    char name[96], path[256], took[48];
    const game_data_t *data;
    if (!game_here(menu, name, sizeof(name), path, sizeof(path), &data)) {
        return false;
    }
    /* Every other time, something particular about it, if there is something. */
    static bool special_turn = true;
    special_turn = !special_turn;
    if (!special_turn && special_line(code_here, name, out, size)) {
        return true;
    }
    for (int k = 0; k < 3; k++) {
        int which = (game_next + k) % 3;
        if (which == 0) {               /* how much it has been played */
            int count = play_stats_get(path, NULL);
            uint32_t seconds = play_stats_seconds(path);
            if (count == 0) {
                snprintf(out, size, snarky() ? "%s, never played. Saving it for a special occasion?"
                                             : "You've never played %s! Why not give it a try?", name);
            } else if (seconds >= 60) {
                duration(seconds, took, sizeof(took));
                snprintf(out, size, snarky() ? "%s: started %d time%s, %s in all. And counting."
                                             : "You've played %s %d time%s, for %s in all!",
                         name, count, (count == 1) ? "" : "s", took);
            } else {
                snprintf(out, size, snarky() ? "%s: started %d time%s. Commitment issues?"
                                             : "You've started %s %d time%s!", name, count, (count == 1) ? "" : "s");
            }
        } else if (which == 1) {        /* its kind of game */
            const genre_line_t *line = NULL;
            for (int i = 0; data && data->genre[0] && i < (int) (sizeof(genre_lines) / sizeof(genre_lines[0])); i++) {
                if (strcmp(genre_lines[i].genre, data->genre) == 0) {
                    line = &genre_lines[i];
                }
            }
            if (!line) {
                continue;
            }
            snprintf(out, size, snarky() ? line->snarky : line->eager, name);
        } else {                        /* how many can play */
            if (!data || data->players < 2) {
                continue;
            }
            if (snarky()) {
                snprintf(out, size, "%s takes up to %d players. You do have friends, right?", name, data->players);
            } else {
                snprintf(out, size, "%s plays with up to %d people. Grab some friends!", name, data->players);
            }
        }
        game_next = (which + 1) % 3;
        return true;
    }
    return false;
}

/* A look back at the play stats. */
static bool line_stats (menu_t *menu, char *out, size_t size) {
    char took[48];
    for (int k = 0; k < 2; k++) {
        int which = (stats_next + k) % 2;
        if (which == 0) {               /* everything played */
            uint32_t seconds;
            int games;
            play_stats_totals(&seconds, &games);
            if (games == 0 || seconds < 60) {
                continue;
            }
            duration(seconds, took, sizeof(took));
            snprintf(out, size, snarky() ? "%d games, %s of your life. Worth every minute? Probably."
                                         : "In all, you've played %d games for %s. Nice!", games, took);
        } else {                        /* the most played game on this row */
            const cover_list_t *covers = cover_list_current(menu);
            if (menu->mode == MENU_MODE_LOAD_ROM || covers->entries <= 0) {
                continue;
            }
            uint32_t best = 0;
            char name[96] = "";
            for (int i = 0; i < covers->entries; i++) {
                path_t *path = covers->path(menu, i);
                uint32_t seconds = play_stats_seconds(path_get(path));
                path_free(path);
                if (seconds > best) {
                    best = seconds;
                    snprintf(name, sizeof(name), "%s", display_name(&covers->list[i]));
                }
            }
            if (best < 60) {
                continue;
            }
            duration(best, took, sizeof(took));
            snprintf(out, size, snarky() ? "%s: %s. We get it, you like it."
                                         : "Your most played game here is %s, at %s!", name, took);
        }
        stats_next = (which + 1) % 2;
        return true;
    }
    return false;
}

/* Any tip that still applies, said before or not. */
static bool line_tip (menu_t *menu, char *out, size_t size) {
    for (int k = 0; k < TIPS_COUNT; k++) {
        int i = (any_tip_next + k) % TIPS_COUNT;
        if (!screen_is(menu, tips[i].where) || tips[i].done(menu)) {
            continue;
        }
        any_tip_next = (i + 1) % TIPS_COUNT;
        snprintf(out, size, "%s", snarky() ? tips[i].snarky : tips[i].eager);
        return true;
    }
    return false;
}

/* The next thing to say while it stays: a game line, a joke, the stats, a
   tip, in turn, skipping any with nothing to say. A joke always has one. */
static void linger_line (menu_t *menu, char *out, size_t size) {
    for (int k = 0; k < 4; k++) {
        int which = (linger_step + k) % 4;
        bool said = (which == 0) ? line_game(menu, out, size)
                  : (which == 2) ? line_stats(menu, out, size)
                  : (which == 3) ? line_tip(menu, out, size)
                  : false;
        if (which == 1) {
            snprintf(out, size, "%s", snarky() ? jokes_snarky[joke_next] : jokes_eager[joke_next]);
            joke_next = (joke_next + 1) % JOKES_COUNT;
            said = true;
        }
        if (said) {
            linger_step = (which + 1) % 4;
            return;
        }
    }
}

/* Opening a game's info page: a word once the cover has landed. */
#define OPEN_AFTER_MS   (1300)
#define OPEN_GAP_MS     (60000)     /* not again within a minute of anything said */
static const char *open_eager[] = { "%s! Great choice!", "Ooh, %s. Let's have a look!", "%s! I've heard good things!" };
static const char *open_snarky[] = { "%s? Bold choice.", "%s. Sure, why not.", "Looking at %s again? Just play it already." };
static int open_next = 0;
static uint64_t opened_at = 0;      /* when a Game info page opened (0: said, or not open) */

static bool line_open (menu_t *menu, char *out, size_t size) {
    char name[96], path[256];
    const game_data_t *data;
    if (!game_here(menu, name, sizeof(name), path, sizeof(path), &data)) {
        return false;
    }
    if (skin_home(code_here) && skin_pick(LINE_OPEN, NULL, out, size)) {
        return true;
    }
    snprintf(out, size, snarky() ? open_snarky[open_next % 3] : open_eager[open_next % 3], name);
    open_next++;
    return true;
}

static int pick_tip (menu_t *menu) {
    for (int k = 0; k < TIPS_COUNT; k++) {
        int i = (next_tip + k) % TIPS_COUNT;
        if ((seen_bits & (1u << i)) || !screen_is(menu, tips[i].where) || tips[i].done(menu)) {
            continue;
        }
        next_tip = (i + 1) % TIPS_COUNT;
        return i;
    }
    return -1;
}

void assistant_update (menu_t *menu) {
    int often = options_get(OPTION_ASSISTANT);
    uint64_t now = get_ticks_ms();

    static int last_mode = -1;
    if ((int) menu->mode != last_mode) {
        last_mode = menu->mode;
        opened_at = (menu->mode == MENU_MODE_LOAD_ROM) ? now : 0;
    }

    if (state != QUIET) {
        if (often == ASSISTANT_OFF || menu->mode != talk_screen
            || (talk_screen != MENU_MODE_LOAD_ROM && !screen_is(menu, WHERE_COVERS))
            || options_get(OPTION_ASSISTANT_SKIN) != skin.choice) {
            let_go();   /* switched off, or the player has moved to another screen */
            return;
        }
        /* Any button sends it off; otherwise it leaves when it has had its
           say, or stays with something new to say while the player is idle. */
        bool pressed = animations_last_press_ms() > state_since;
        bool idle_long = (now - animations_last_press_ms()) >= LINGER_AFTER_MS && !animations_popup_up();
        uint64_t say_for = (lingering || idle_long) ? LINGER_SAY_MS : SAY_MS;
        if ((state == ARRIVING || state == TALKING) && pressed) {
            if (lingering) {
                last_talk_ms = now;     /* the usual gap from here */
            }
            lingering = false;
            state = LEAVING;
            state_since = now;
        } else if (state == TALKING && now - state_since >= say_for) {
            if (idle_long) {
                /* Still idle: stay, with something new to say. */
                char text[TEXT_LENGTH];
                linger_line(menu, text, sizeof(text));
                lingering = true;
                set_words(text, true);
                state_since = now;
                debugf("assistant: %s\n", text);
            } else {
                lingering = false;
                state = LEAVING;
                state_since = now;
            }
        } else if (state == ARRIVING && now - state_since >= ARRIVE_MS) {
            state = TALKING;
            state_since = now;
            if (saying_tip >= 0 || saying_bits) {
                if (saying_tip >= 0) {
                    seen_bits |= 1u << saying_tip;  /* only now: sent off as it flew in, it is said another time */
                }
                said_bits |= saying_bits;
                save();
            }
        } else if (state == LEAVING && now - state_since >= LEAVE_MS) {
            let_go();
        }
        return;
    }

    if (dirty && now - animations_last_press_ms() >= QUIET_MS && safe_file_idle_turn()) {
        save();     /* a feature used since: so it isn't suggested after a restart */
    }
    if (often == ASSISTANT_OFF || (!screen_is(menu, WHERE_COVERS) && !screen_is(menu, WHERE_GAME_INFO))) {
        return;
    }
    load();
    if (!started_checked && screen_is(menu, WHERE_COVERS)) {
        started_checked = true;
        started_check(menu);
    }

    /* Something that just happened: soon, once the player has paused. The
       welcomes (back from a game, good morning) come first; nothing else is
       said until they have been, nor in the first 45 seconds. */
    uint64_t quiet = now - animations_last_press_ms();
    bool welcoming = false;
    int due = -1;
    for (int i = 0; i < pending_count; i++) {
        if (pending[i].not_before == 0) {
            welcoming = true;
        }
        if (due < 0 && pending[i].not_before <= now) {
            due = i;
        }
    }
    if (due >= 0 && screen_is(menu, WHERE_COVERS) && now >= EARLY_MS && quiet >= REACT_QUIET_MS
        && !animations_popup_up() && (!last_talk_ms || now - last_talk_ms >= REACT_GAP_MS)) {
        saying_tip = -1;
        saying_bits = pending[due].bits;
        last_talk_ms = now;
        talk_screen = menu->mode;
        say(pending[due].text);
        debugf("assistant: %s\n", pending[due].text);
        pending_count--;
        memmove(&pending[due], &pending[due + 1], sizeof(pending[0]) * (pending_count - due));
        return;
    }
    if (welcoming || now < FIRST_TALK_MS) {
        return;
    }

    /* A Game info page just opened: a word about the game. */
    if (opened_at && now - opened_at >= OPEN_AFTER_MS && quiet >= 1000 && !animations_popup_up()) {
        opened_at = 0;
        char text[TEXT_LENGTH];
        if ((!last_talk_ms || now - last_talk_ms >= OPEN_GAP_MS) && line_open(menu, text, sizeof(text))) {
            saying_tip = -1;
            saying_bits = 0;
            last_talk_ms = now;
            talk_screen = menu->mode;
            say(text);
            debugf("assistant: opening, %s\n", text);
            return;
        }
    }

    /* Resting on a game: something particular about it (the skin's own
       games, another one it knows, a well-known game), once per game in a
       row, not too soon after anything else. Chatty: every game rested on
       for 5 seconds gets a word, particular or not, with no gap. */
    skin_load();
    bool chatty = (often == ASSISTANT_CHATTY);
    if (quiet >= (chatty ? CHATTY_REST_MS : HOME_QUIET_MS) && !animations_popup_up() && menu->mode != MENU_MODE_LOAD_ROM
        && (chatty || !last_talk_ms || now - last_talk_ms >= (often == ASSISTANT_RARELY ? 3 * HOME_GAP_MS + 45000 : HOME_GAP_MS))) {
        static int checked_selected = -2;
        static const void *checked_list = NULL;
        static uint32_t said_key = 0;
        const cover_list_t *covers = cover_list_current(menu);
        const void *list = (menu->mode == MENU_MODE_LOAD_ROM) ? (const void *) menu->load.rom_path : (const void *) covers->list;
        int selected = (menu->mode == MENU_MODE_LOAD_ROM) ? -1 : covers->selected;
        if (selected != checked_selected || list != checked_list) {
            checked_selected = selected;
            checked_list = list;
            char name[96], path[256], text[TEXT_LENGTH];
            const game_data_t *data;
            if (game_here(menu, name, sizeof(name), path, sizeof(path), &data)) {
                uint32_t key = 2166136261u;
                for (const unsigned char *c = (const unsigned char *) path; *c; c++) {
                    key = (key ^ *c) * 16777619u;
                }
                if (key != said_key && (special_line(code_here, name, text, sizeof(text))
                                        || (chatty && line_game(menu, text, sizeof(text))))) {
                    said_key = key;
                    saying_tip = -1;
                    saying_bits = 0;
                    last_talk_ms = now;
                    talk_screen = menu->mode;
                    say(text);
                    debugf("assistant: about the game, %s\n", text);
                    return;
                }
            }
        }
    }

    /* Idle for a minute: come and stay. */
    if (quiet >= LINGER_AFTER_MS && !animations_popup_up()) {
        char text[TEXT_LENGTH];
        linger_line(menu, text, sizeof(text));
        lingering = true;
        saying_tip = -1;
        saying_bits = 0;
        talk_screen = menu->mode;
        say(text);
        debugf("assistant: stays, %s\n", text);
        return;
    }

    if (now < FIRST_TALK_MS || (last_talk_ms && now - last_talk_ms < gap_ms(often))) {
        return;
    }
    if (quiet < QUIET_MS || animations_popup_up()) {
        return;
    }
    if (screen_is(menu, WHERE_COVERS) && unplayed_check(menu)) {
        return;     /* said next time round, as a reaction */
    }
    int tip = pick_tip(menu);
    if (tip < 0) {
        last_talk_ms = now;     /* nothing to say here: look again after the usual gap */
        return;
    }
    const char *text = (options_get(OPTION_ASSISTANT_MOOD) == ASSISTANT_SNARKY) ? tips[tip].snarky : tips[tip].eager;
    saying_tip = tip;
    saying_bits = 0;
    last_talk_ms = now;
    talk_screen = menu->mode;
    say(text);
    debugf("assistant: tip %d\n", tip);
}

/* ---------- drawing ---------- */

static float ease_out (float t) {
    float rest = 1.0f - t;
    return 1.0f - rest * rest * rest;
}

/* How it comes and goes, how it moves while it stays, and how its glow
   breathes (Settings > Look > Assistant > Fairy Look...). */
enum { ARRIVAL_FLY, ARRIVAL_SPARKLE, ARRIVAL_FADE, ARRIVAL_DROP };
enum { IDLE_BOB, IDLE_STILL, IDLE_WANDER, IDLE_BOUNCE };
enum { GLOW_STEADY, GLOW_GENTLE, GLOW_STRONG };

const char *assistant_arrival_name (int choice) {
    static const char *names[ASSISTANT_ARRIVAL_COUNT] = { "Fly In", "Sparkle", "Fade", "Drop In" };
    return (choice >= 0 && choice < ASSISTANT_ARRIVAL_COUNT) ? names[choice] : "";
}

const char *assistant_idle_name (int choice) {
    static const char *names[ASSISTANT_IDLE_COUNT] = { "Bob", "Still", "Wander", "Bounce" };
    return (choice >= 0 && choice < ASSISTANT_IDLE_COUNT) ? names[choice] : "";
}

const char *assistant_glow_name (int choice) {
    static const char *names[ASSISTANT_GLOW_COUNT] = { "Off", "Gentle", "Strong" };
    return (choice >= 0 && choice < ASSISTANT_GLOW_COUNT) ? names[choice] : "";
}

/* Where the fairy is, how large and how solid, `e` (0 to 1) of the way
   through coming (or going), with its resting place at rest_x, rest_y.
   burst: how far a sparkle burst has gone (-1 for none). */
typedef struct {
    float x, y, scale;
    uint8_t alpha;
    float burst;
} pose_t;

static pose_t pose (bool coming, float e, float rest_x, float rest_y, int size) {
    pose_t p = { rest_x, rest_y, 1.0f, 255, -1.0f };
    float off_x = display_get_width() + size;
    switch (options_get(OPTION_ASSISTANT_ARRIVAL)) {
        case ARRIVAL_SPARKLE:       /* pops out of a burst of sparkles, and back into one */
            p.scale = coming ? animations_ease_back(e) : 1.0f - (e * e);
            p.burst = e;
            break;
        case ARRIVAL_FADE:
            p.alpha = (uint8_t) (255.0f * (coming ? e : 1.0f - e));
            break;
        case ARRIVAL_DROP:          /* drops from above with a little bounce, and rises away */
            p.y = coming ? -size + ((rest_y + size) * animations_ease_back(e)) : rest_y - ((rest_y + size) * e * e);
            break;
        default: {                  /* flies in from the right, rising, and off up and away */
            float k = coming ? ease_out(e) : e * e;
            p.x = coming ? off_x + ((rest_x - off_x) * k) : rest_x + ((off_x - rest_x) * k);
            p.y = coming ? (rest_y + 40) - (40 * k) : rest_y - (60 * k);
            break;
        }
    }
    return p;
}

/* How it moves while it stays. */
static void idle_offset (uint64_t now, float *dx, float *dy) {
    float t = (float) now;
    *dx = 0.0f;
    *dy = 0.0f;
    switch (options_get(OPTION_ASSISTANT_IDLE)) {
        case IDLE_STILL:
            break;
        case IDLE_WANDER: {         /* a slow figure-eight round its perch */
            float w = t * 2.0f * (float) M_PI / 4200.0f;
            *dx = 10.0f * sinf(w);
            *dy = 5.0f * sinf(2.0f * w);
            break;
        }
        case IDLE_BOUNCE:           /* little hops */
            *dy = -8.0f * fabsf(sinf(t * (float) M_PI / 900.0f));
            break;
        default:
            *dy = 3.0f * sinf(t * 0.0039f);    /* about 1.6 s a bob */
            break;
    }
}

/* Sparkles flying out from the fairy (the Sparkle arrival), `e` of the way. */
#define BURST_STARS     (8)

static void burst_draw (sprite_t *star, float x, float y, int size, float e) {
    if (!star || e < 0.0f || e >= 1.0f) {
        return;
    }
    color_t tint = fairy_tint();
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_filter(FILTER_BILINEAR);
        uint8_t fade = (uint8_t) (255.0f * (1.0f - e));
        for (int i = 0; i < BURST_STARS; i++) {
            float a = (i * 2.0f * (float) M_PI / BURST_STARS) + 0.4f;
            float r = size * (0.25f + (0.75f * e));
            float s = 12.0f * (1.0f - (0.4f * e));
            color_t c = (i & 1) ? tint : RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
            rdpq_set_prim_color(RGBA32(c.r, c.g, c.b, fade));
            rdpq_sprite_blit(star, (int) (x + (r * cosf(a)) - (s / 2)), (int) (y + (r * sinf(a)) - (s / 2)),
                             &(rdpq_blitparms_t) { .scale_x = s / 16.0f, .scale_y = s / 16.0f });
        }
    rdpq_mode_pop();
}

/* The fairy's picture, its middle at x, y, `size` pixels high: a skin turns
   back and forth like a coin, Vee flaps its wings. */
#define GLOW_PATH       "rom:/glow.sprite"  /* a soft round glow, tinted (make_fairy.py) */
#define GLOW_SCALE      (1.9f)              /* across, against the fairy's size */
#define GLOW_ALPHA      (140)

static void picture_draw (sprite_t *picture, float x, float y, int size, uint64_t now, uint8_t alpha) {
    static sprite_t *glow = NULL;   /* 1 KB, kept once loaded */
    if (!glow) {
        glow = sprite_load(GLOW_PATH);
    }
    color_t tint = fairy_tint();
    fonts_set_fairy_color(tint);    /* its name before its words */
    if (size < 2) {
        return;
    }

    /* Glow Pulse: the glow breathes, a little larger and brighter, then back. */
    float wave = sinf((float) now * 2.0f * (float) M_PI / 2400.0f);
    float glow_alpha = GLOW_ALPHA, glow_scale = GLOW_SCALE;
    if (options_get(OPTION_ASSISTANT_GLOW) == GLOW_GENTLE) {
        glow_alpha += 45.0f * wave;
        glow_scale *= 1.0f + (0.05f * wave);
    } else if (options_get(OPTION_ASSISTANT_GLOW) == GLOW_STRONG) {
        glow_alpha = 150.0f + (90.0f * wave);
        glow_scale *= 1.0f + (0.14f * wave);
    }
    glow_alpha *= alpha / 255.0f;

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_filter(FILTER_BILINEAR);

        /* A glow in its colour behind it, for every fairy. */
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
        rdpq_set_prim_color(RGBA32(tint.r, tint.g, tint.b, (uint8_t) glow_alpha));
        float glow_size = size * glow_scale;
        float k = glow_size / 32.0f;
        rdpq_sprite_blit(glow, (int) (x - glow_size / 2), (int) (y - glow_size / 2), &(rdpq_blitparms_t) { .scale_x = k, .scale_y = k });

        if (skin.choice > 0) {
            rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,TEX0), (TEX0,0,PRIM,0)));    /* a skin's own picture, as drawn (fading) */
            rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, alpha));
            /* A skin has one picture: it faces one way, then turns (squashed
               through the middle, like a coin) to face the other, and back. */
            float scale = (float) size / SKIN_SIZE;
            int cycle = (int) (now % (2 * (TURN_EVERY_MS + TURN_MS)));
            int half = TURN_EVERY_MS + TURN_MS;
            int phase = cycle % half;
            float facing = (phase < TURN_EVERY_MS) ? 1.0f : cosf((float) M_PI * (phase - TURN_EVERY_MS) / TURN_MS);
            if (cycle >= half) {
                facing = -facing;
            }
            float across = fabsf(facing) < 0.06f ? 0.06f : fabsf(facing);
            int width = (int) (size * across);
            rdpq_sprite_blit(picture, (int) x - width / 2, (int) y - size / 2, &(rdpq_blitparms_t) {
                .scale_x = scale * across,
                .scale_y = scale,
                .flip_x = facing < 0.0f,
            });
        } else {
            /* Vee, its wings flapping: two pictures side by side, drawn larger.
               Its shades of white are painted in its colour: the darkest
               parts the colour itself, its heart white. */
            rdpq_mode_combiner(RDPQ_COMBINER1((1,PRIM,TEX0,PRIM), (TEX0,0,PRIM,0)));
            rdpq_set_prim_color(RGBA32(tint.r, tint.g, tint.b, alpha));
            int frame = (int) ((now / FLAP_MS) % 2);
            float scale = size / (float) FAIRY_SIZE;
            rdpq_sprite_blit(picture, (int) x - size / 2, (int) y - size / 2, &(rdpq_blitparms_t) {
                .s0 = frame * FAIRY_SIZE,
                .width = FAIRY_SIZE,
                .scale_x = scale,
                .scale_y = scale,
            });
        }
    rdpq_mode_pop();
}

void assistant_draw (menu_t *menu) {
    if (state == QUIET || menu->mode != talk_screen || !fairy || !words) {
        return;
    }
    uint64_t now = get_ticks_ms();
    float t = (float) (now - state_since);
    fonts_set_fairy_color(fairy_tint());    /* its name, drawn before the fairy */

    /* The band: the hints' place, grown upward if the words need a third line. */
    int bx0, by0, bx1, by1;
    games_ui_hints_rect(&bx0, &by0, &bx1, &by1);
    int tallest = (old_words && old_height > words_height) ? old_height : words_height;
    int words_needed = tallest + (BAND_PAD * 2) - 4;
    if (by1 - by0 < words_needed) {
        by0 = (by1 - words_needed) & ~1;
    }

    /* Where the fairy rests: perched on the band's top right corner, below
       the right-hand cover, clear of the title panel and position bar. */
    float rest_x = PERCH_X;
    float rest_y = by0 - PERCH_ABOVE;

    /* Coming and going as the player chose (Arrival), moving while it stays (Idle Motion). */
    pose_t p = { rest_x, rest_y, 1.0f, 255, -1.0f };
    if (state == ARRIVING) {
        p = pose(true, t / ARRIVE_MS > 1.0f ? 1.0f : t / ARRIVE_MS, rest_x, rest_y, PERCH_SIZE);
    } else if (state == LEAVING) {
        p = pose(false, t / LEAVE_MS > 1.0f ? 1.0f : t / LEAVE_MS, rest_x, rest_y, PERCH_SIZE);
    }
    float dx, dy;
    idle_offset(now, &dx, &dy);
    float x = p.x + dx, y = p.y + dy;

    /* The words, in the band, while it talks: the hints under it are covered. */
    if (state == TALKING) {
        const theme_t *th = theme_get();
        games_ui_round_fill(bx0 - 2, by0 - 2, bx1 + 2, by1 + 2, fairy_tint());
        games_ui_round_fill(bx0, by0, bx1, by1, RGBA32(th->panel.r, th->panel.g, th->panel.b, 0xFF));
        int text_y = (by0 + ((by1 - by0) - words_height) / 2) & ~1;
        float swap = old_words ? animations_progress(swap_since, SWAP_MS) : 1.0f;
        if (swap >= 1.0f) {
            if (old_words) {
                rdpq_paragraph_free(old_words);
                old_words = NULL;
            }
            rdpq_paragraph_render(words, bx0 + BAND_PAD, text_y);
        } else {
            /* Showy: the old line slides away left as the new one sweeps
               in from the right, with sparkles where the two meet. */
            float e = ease_out(swap);
            int width = bx1 - bx0;
            int edge = (int) ((1.0f - e) * width) & ~1;
            int old_y = (by0 + ((by1 - by0) - old_height) / 2) & ~1;
            rdpq_set_scissor(bx0 + 2, by0, bx1 - 2, by1);
            rdpq_paragraph_render(old_words, bx0 + BAND_PAD - ((width - edge) & ~1), old_y);
            rdpq_paragraph_render(words, bx0 + BAND_PAD + edge, text_y);
            /* Little stars, white and the fairy's colour in turn, twinkling
               along the edge and fading as the new line settles. */
            rdpq_mode_push();
                rdpq_set_mode_standard();
                rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
                rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                rdpq_mode_filter(FILTER_BILINEAR);
                uint8_t fade = (uint8_t) (255.0f * (1.0f - swap * swap));
                for (int i = 0; i < SPARKLES; i++) {
                    color_t tint = (i & 1) ? fairy_tint() : RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
                    rdpq_set_prim_color(RGBA32(tint.r, tint.g, tint.b, fade));
                    float twinkle = 0.65f + 0.35f * sinf((float) now * 0.03f + i * 1.7f);
                    float size = (8 + (i * 29) % 7) * twinkle;
                    int sx = bx0 + edge + ((i * 53) % 32) - 16;
                    int sy = by0 + 4 + (i * 37) % ((by1 - by0) - 8);
                    float k = size / 16.0f;
                    rdpq_sprite_blit(sparkle, sx - (int) (size / 2), sy - (int) (size / 2), &(rdpq_blitparms_t) {
                        .scale_x = k,
                        .scale_y = k,
                    });
                }
            rdpq_mode_pop();
            rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
        }
    }

    burst_draw(sparkle, x, y, PERCH_SIZE, p.burst);
    picture_draw(fairy, x, y, (int) (PERCH_SIZE * p.scale), now, p.alpha);
}

/* ---------- for the Ask page (assistant_ask.c) ---------- */

static sprite_t *portrait = NULL;   /* its own copy: the perch's comes and goes */
static sprite_t *portrait_star = NULL;
static int portrait_choice = -1;
static int portrait_arrival = -1;   /* the Arrival it last showed: a change shows it again */
static uint64_t portrait_since = 0;

const char *assistant_name (void) {
    return fairy_name();
}

color_t assistant_color (void) {
    skin_load();
    return fairy_tint();
}

bool assistant_snarky (void) {
    return snarky();
}

void assistant_portrait_draw (int x, int y, int size) {
    skin_load();
    if (portrait && portrait_choice != skin.choice) {
        assistant_portrait_free();
    }
    if (!portrait) {
        char path[64];
        if (skin.choice > 0) {
            snprintf(path, sizeof(path), "rom:/assistant/%s.sprite", skin_files[skin.choice - 1]);
        } else {
            snprintf(path, sizeof(path), "%s", FAIRY_PATH);
        }
        portrait = sprite_load(path);
        portrait_star = sprite_load(SPARKLE_PATH);
        portrait_choice = skin.choice;
        portrait_since = get_ticks_ms();    /* it arrives as it would on the covers */
    }
    uint64_t now = get_ticks_ms();
    if (options_get(OPTION_ASSISTANT_ARRIVAL) != portrait_arrival) {
        portrait_arrival = options_get(OPTION_ASSISTANT_ARRIVAL);
        portrait_since = now;
    }
    float e = (float) (now - portrait_since) / ARRIVE_MS;
    pose_t p = { (float) x, (float) y, 1.0f, 255, -1.0f };
    if (e < 1.0f) {
        p = pose(true, e, (float) x, (float) y, size);
    }
    float dx, dy;
    idle_offset(now, &dx, &dy);
    burst_draw(portrait_star, p.x + dx, p.y + dy, size, p.burst);
    picture_draw(portrait, p.x + dx, p.y + dy, (int) (size * p.scale), now, p.alpha);
}

void assistant_portrait_free (void) {
    if (portrait) {
        rspq_wait();    /* the chip may still be drawing it */
        sprite_free(portrait);
        portrait = NULL;
        if (portrait_star) {
            sprite_free(portrait_star);
            portrait_star = NULL;
        }
        portrait_arrival = -1;
    }
}
