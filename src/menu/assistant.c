#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "animations.h"
#include "assistant.h"
#include "fonts.h"
#include "games_ui.h"
#include "menu_features.h"
#include "menu_options.h"
#include "my_collections.h"
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

/* The bubble sits at the bottom right, over the title panel; the fairy
   lands in a space kept clear at its right end, so it is always seen on
   the bubble's own plain fill (its pale wings ran into the text under it). */
#define BUBBLE_X1       (GAMES_UI_CONTENT_X1)
#define BUBBLE_WIDTH    (340)
#define FAIRY_ROOM      (56)        /* the bubble's right end, kept for the fairy */
#define BUBBLE_PAD      (10)
#define NAME_HEIGHT     (20)
#define BUBBLE_BOTTOM   (364)
#define TEXT_WIDTH      (BUBBLE_WIDTH - FAIRY_ROOM - (BUBBLE_PAD * 2))
#define REST_X          (BUBBLE_X1 - (FAIRY_ROOM / 2) - 2)     /* where the fairy hovers while it talks: its middle */

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
    used_bits = seen_bits = 0;
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
    fprintf(f, "%s\nseen %08lx\nused %08lx\n", FILE_HEADER, (unsigned long) seen_bits, (unsigned long) used_bits);
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
    return menu->mode == MENU_MODE_BROWSER || menu->mode == MENU_MODE_HISTORY || menu->mode == MENU_MODE_FAVORITE;
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

static void say (const char *text) {
    if (!fairy) {
        fairy = sprite_load(FAIRY_PATH);
    }
    if (words) {
        rdpq_paragraph_free(words);
    }
    int bytes = strlen(text);
    words = rdpq_paragraph_build(&(rdpq_textparms_t) {
        .width = TEXT_WIDTH,
        .height = 200,
        .wrap = WRAP_WORD,
        .style_id = STL_DEFAULT,
    }, FNT_DEFAULT, text, &bytes);
    words_height = (int) (words->bbox.y1 - words->bbox.y0);
    state = ARRIVING;
    state_since = get_ticks_ms();
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
        if (often == ASSISTANT_OFF || menu->mode != talk_screen) {
            let_go();   /* switched off, or the player has moved to another screen */
            return;
        }
        /* Any button sends it off; otherwise it leaves when it has had its say. */
        bool pressed = animations_last_press_ms() > state_since;
        if ((state == ARRIVING || state == TALKING) && (pressed || (state == TALKING && now - state_since >= SAY_MS))) {
            state = LEAVING;
            state_since = now;
        } else if (state == ARRIVING && now - state_since >= ARRIVE_MS) {
            state = TALKING;
            state_since = now;
            if (saying_tip >= 0) {
                seen_bits |= 1u << saying_tip;  /* only now: sent off as it flew in, it is said another time */
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
    if (often == ASSISTANT_OFF || now < FIRST_TALK_MS || (last_talk_ms && now - last_talk_ms < gap_ms(often))) {
        return;
    }
    if (now - animations_last_press_ms() < QUIET_MS || animations_popup_up()) {
        return;
    }
    load();
    int tip = pick_tip(menu);
    if (tip < 0) {
        last_talk_ms = now;     /* nothing to say here: look again after the usual gap */
        return;
    }
    const char *text = (options_get(OPTION_ASSISTANT_MOOD) == ASSISTANT_SNARKY) ? tips[tip].snarky : tips[tip].eager;
    saying_tip = tip;
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

    /* Where the fairy is: flying in from the right, hovering with a gentle
       bob, then off up and away. It rests in the middle of the bubble's
       height, so the bubble's size sets where. */
    int height = BUBBLE_PAD + NAME_HEIGHT + words_height + BUBBLE_PAD;
    int rest_y = BUBBLE_BOTTOM - (height / 2);
    float x = REST_X, y = rest_y;
    float off_x = display_get_width() + FAIRY_SIZE;
    if (state == ARRIVING) {
        float e = ease_out(t / ARRIVE_MS > 1.0f ? 1.0f : t / ARRIVE_MS);
        x = off_x + (REST_X - off_x) * e;
        y = (rest_y - 70) + 70 * e;
    } else if (state == LEAVING) {
        float e = t / LEAVE_MS > 1.0f ? 1.0f : t / LEAVE_MS;
        e = e * e;
        x = REST_X + (off_x - REST_X) * e;
        y = rest_y - 110 * e;
    }
    y += 3.0f * sinf((float) now * 0.0039f);    /* about 1.6 s a bob */

    /* The bubble, while it talks: the name, then the words. */
    if (state == TALKING) {
        const theme_t *th = theme_get();
        int x1 = BUBBLE_X1;
        int x0 = x1 - BUBBLE_WIDTH;
        int y1 = BUBBLE_BOTTOM;
        int y0 = y1 - height;
        games_ui_round_fill(x0 - 2, y0 - 2, x1 + 2, y1 + 2, th->accent);
        games_ui_round_fill(x0, y0, x1, y1, RGBA32(th->panel.r, th->panel.g, th->panel.b, 0xFF));
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = TEXT_WIDTH,
            .height = NAME_HEIGHT,
            .valign = VALIGN_CENTER,
            .style_id = STL_ORANGE,
        }, FNT_DEFAULT, x0 + BUBBLE_PAD, y0 + BUBBLE_PAD - 2, "%s", FAIRY_NAME);
        rdpq_paragraph_render(words, x0 + BUBBLE_PAD, y0 + BUBBLE_PAD + NAME_HEIGHT);
    }

    /* The fairy, its wings flapping: two pictures side by side. */
    int frame = (int) ((now / FLAP_MS) % 2);
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_sprite_blit(fairy, (int) x - FAIRY_SIZE / 2, (int) y - FAIRY_SIZE / 2, &(rdpq_blitparms_t) {
            .s0 = frame * FAIRY_SIZE,
            .width = FAIRY_SIZE,
        });
    rdpq_mode_pop();
}
