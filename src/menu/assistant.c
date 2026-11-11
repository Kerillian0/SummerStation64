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
#define FAIRY_SIZE      (40)        /* each of its two pictures (wings up, wings down) */
#define FAIRY_NAME      "Spark"
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
#define FAIRY_DRAW      (52)        /* drawn this size, from its 40 px pictures */
#define CORNER_X        (VISIBLE_AREA_X1 - 16 - 30)    /* its middle: the badge's place, inside the safe area */
#define CORNER_Y        (GAMES_UI_TOPBAR_BOTTOM + 34)  /* bob and all, clear of the covers below */
#define BAND_PAD        (12)
#define BAND_FAIRY_ROOM (60)        /* Game info: the band's right end, kept for the fairy */
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
    { WHERE_COVERS, "Want more sparkle? Settings > Display > Animations: try Showy!",
                    "This menu can dance, you know. Settings > Display > Animations > Showy.", done_showy },
    { WHERE_COVERS, "Hold START for a second to take a screenshot. They go in menu/screenshots!",
                    "Hold START a second for a screenshot. Proof, for when nobody believes you.", done_screenshot },
    { WHERE_GAME_INFO, "Press START here to see all of this game's details!",
                       "START shows the details. All the nerdy stuff. You'll love it.", done_details },
    { WHERE_COVERS, "Make the menu your own! Settings > Display > Edit Theme.",
                    "Bored of these colors? Settings > Display > Edit Theme. Go wild.", done_theme },
    { WHERE_COVERS, "Sharing this console? Settings > Profiles gives every player their own saves!",
                    "Someone else playing on your saves? Settings > Profiles. Fix that.", done_profiles },
    { WHERE_COVERS, "Missing some box art? The Companion web page can fill it in for you!",
                    "No box art? The Companion web page does it for you. Lazy, but smart.", done_never },
};
#define TIPS_COUNT  ((int) (sizeof(tips) / sizeof(tips[0])))

/* ---------- the player's file ---------- */

static int loaded_for = -1;     /* profiles_generation() */
static bool dirty = false;

static void load (void) {
    if (loaded_for == profiles_generation()) {
        return;
    }
    loaded_for = profiles_generation();
    used_bits = seen_bits = said_bits = 0;
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
    bool ok = (fclose(f) == 0);
    if (!ok || !safe_file_replace(tmp, path)) {
        debugf("assistant: could not replace %s\n", path);
        return;
    }
    dirty = false;
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
static sprite_t *fairy = NULL;
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
    if (fairy) {
        sprite_free(fairy);
        fairy = NULL;
    }
    state = QUIET;
}

static bool in_corner (void) {
    return talk_screen != MENU_MODE_LOAD_ROM;
}

bool assistant_in_corner (void) {
    return state != QUIET && in_corner();
}

/* The text's width in the band. */
static int band_text_width (void) {
    int x0, y0, x1, y1;
    games_ui_hints_rect(&x0, &y0, &x1, &y1);
    return (x1 - x0) - (BAND_PAD * 2) - (in_corner() ? 0 : BAND_FAIRY_ROOM);
}

/* Lay out what it says, in the band's width: its name in orange first. */
static void set_words (const char *message) {
    if (words) {
        rdpq_paragraph_free(words);
    }
    /* Its name in orange first, then what it says. */
    char text[TEXT_LENGTH + 32];
    snprintf(text, sizeof(text), "^%02X%s:^00 %s", STL_ORANGE, FAIRY_NAME, message);
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
    if (!fairy) {
        fairy = sprite_load(FAIRY_PATH);
    }
    set_words(message);
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

#define PENDING_MAX     (3)
static struct {
    char text[TEXT_LENGTH];
    uint32_t bits;          /* milestones it says (marked once it is talking) */
} pending[PENDING_MAX];
static int pending_count = 0;
static bool started_checked = false;

static bool snarky (void) {
    return options_get(OPTION_ASSISTANT_MOOD) == ASSISTANT_SNARKY;
}

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
static void started_check (menu_t *menu) {
    char name[96], took[48], total[48];
    const char *rom;
    uint32_t length, count, seconds;
    if (play_stats_settled(&rom, &length, &count, &seconds)) {
        game_name(rom, name, sizeof(name));
        const char *dot = strrchr(rom, '.');
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
        }
    }

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

    if (menu->current_time > 0) {
        struct tm *now = localtime(&menu->current_time);
        if (now && now->tm_hour < 4) {
            queue(0, "%s", snarky()
                ? "It's past midnight. The games will still be here tomorrow. Probably."
                : "It's getting late! One more game, then off to bed?");
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
static bool game_here (menu_t *menu, char *name, size_t name_size, char *path_out, size_t path_size, const game_data_t **data) {
    *data = NULL;
    if (menu->mode == MENU_MODE_LOAD_ROM) {
        if (!menu->load.rom_path) {
            return false;
        }
        snprintf(path_out, path_size, "%s", path_get(menu->load.rom_path));
        game_name(path_out, name, name_size);
        *data = game_data_find(menu->load.rom_info.game_code);
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
    *data = known ? game_data_find(known->code) : NULL;
    return true;
}

/* Something about the game it is resting on. */
static bool line_game (menu_t *menu, char *out, size_t size) {
    char name[96], path[256], took[48];
    const game_data_t *data;
    if (!game_here(menu, name, sizeof(name), path, sizeof(path), &data)) {
        return false;
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

    if (state != QUIET) {
        if (often == ASSISTANT_OFF || menu->mode != talk_screen || (in_corner() && !screen_is(menu, WHERE_COVERS))) {
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
                set_words(text);
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

    /* Something that just happened: soon, once the player has paused. */
    uint64_t quiet = now - animations_last_press_ms();
    if (pending_count > 0 && screen_is(menu, WHERE_COVERS) && now >= EARLY_MS && quiet >= REACT_QUIET_MS
        && !animations_popup_up() && (!last_talk_ms || now - last_talk_ms >= REACT_GAP_MS)) {
        saying_tip = -1;
        saying_bits = pending[0].bits;
        last_talk_ms = now;
        talk_screen = menu->mode;
        say(pending[0].text);
        debugf("assistant: %s\n", pending[0].text);
        pending_count--;
        memmove(&pending[0], &pending[1], sizeof(pending[0]) * pending_count);
        return;
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

void assistant_draw (menu_t *menu) {
    if (state == QUIET || menu->mode != talk_screen || !fairy || !words) {
        return;
    }
    uint64_t now = get_ticks_ms();
    float t = (float) (now - state_since);

    /* The band: the hints' place, grown upward if the words need a third line. */
    int bx0, by0, bx1, by1;
    games_ui_hints_rect(&bx0, &by0, &bx1, &by1);
    int words_needed = words_height + (BAND_PAD * 2) - 4;
    if (by1 - by0 < words_needed) {
        by0 = (by1 - words_needed) & ~1;
    }

    /* Where the fairy rests: the corner, or the band's right end. */
    float rest_x = in_corner() ? CORNER_X : (bx1 - (BAND_FAIRY_ROOM / 2) - 4);
    float rest_y = in_corner() ? CORNER_Y : ((by0 + by1) / 2);

    /* Flying in from the right, hovering with a gentle bob, then off up and away. */
    float x = rest_x, y = rest_y;
    float off_x = display_get_width() + FAIRY_DRAW;
    if (state == ARRIVING) {
        float e = ease_out(t / ARRIVE_MS > 1.0f ? 1.0f : t / ARRIVE_MS);
        x = off_x + (rest_x - off_x) * e;
        y = (rest_y + 40) - 40 * e;
    } else if (state == LEAVING) {
        float e = t / LEAVE_MS > 1.0f ? 1.0f : t / LEAVE_MS;
        e = e * e;
        x = rest_x + (off_x - rest_x) * e;
        y = rest_y - 60 * e;
    }
    y += 3.0f * sinf((float) now * 0.0039f);    /* about 1.6 s a bob */

    /* The words, in the band, while it talks: the hints under it are covered. */
    if (state == TALKING) {
        const theme_t *th = theme_get();
        games_ui_round_fill(bx0 - 2, by0 - 2, bx1 + 2, by1 + 2, th->accent);
        games_ui_round_fill(bx0, by0, bx1, by1, RGBA32(th->panel.r, th->panel.g, th->panel.b, 0xFF));
        int text_y = by0 + ((by1 - by0) - words_height) / 2;
        rdpq_paragraph_render(words, bx0 + BAND_PAD, text_y & ~1);
    }

    /* The fairy, its wings flapping: two pictures side by side, drawn larger. */
    int frame = (int) ((now / FLAP_MS) % 2);
    float scale = (in_corner() ? FAIRY_DRAW : FAIRY_SIZE) / (float) FAIRY_SIZE;
    int size = (int) (FAIRY_SIZE * scale);
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_sprite_blit(fairy, (int) x - size / 2, (int) y - size / 2, &(rdpq_blitparms_t) {
            .s0 = frame * FAIRY_SIZE,
            .width = FAIRY_SIZE,
            .scale_x = scale,
            .scale_y = scale,
        });
    rdpq_mode_pop();
}
