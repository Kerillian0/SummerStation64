/**
 * @file intro.c
 * @brief The short intro shown when the console is switched on.
 */

#include <libdragon.h>
#include "theme_pack.h"
#include "assistant.h"
#include "cover_deal.h"
#include "animations.h"
#include "volume.h"
#include "memory_note.h"
#include "flashcart/flashcart.h"
#include "fonts.h"
#include "intro.h"
#include "intro_logo.h"
#include "menu_name.h"
#include "menu_options.h"
#include "safe_mode.h"
#include "sound.h"
#include "theme.h"
#include "ui_components.h"
#include "ui_components/constants.h"

/* All times are in milliseconds from the first frame. */
#define INTRO_FADE_IN_MS    (400)   /* the picture comes up from black */
#define INTRO_BAR_START_MS  (250)   /* the line under the name starts to grow */
#define INTRO_BAR_GROW_MS   (650)
#define INTRO_FADE_OUT_MS   (300)   /* and goes back to black at the end */
#define INTRO_SHORTEST_MS   (2600)  /* the intro lasts as long as its tune (intro.wav), but at least this */
#define INTRO_LONGEST_MS    (8000)  /* and never more than this, however long the tune is */

#define INTRO_CHANNEL       (SOUND_SFX_CHANNEL + 2)     /* sound effect channels the menu doesn't use (a stereo tune takes two) */

/* The fade-in (option `fade`): when the first screen appears, with or
   without the intro before it, the picture comes up from black and the
   background music (if the player has it on) rises from silence, together. */
#define FADE_MS             (2000)
#define FADE_FAST_MS        (1000)
#define FADE_WAIT_MS        (1000)  /* if no screen has drawn the fade by then, the music starts rising anyway */

/* The logo turns steadily the whole time, as it does at the start of
   Ocarina of Time. */
#define LOGO_Y              (160)   /* its middle */
#define LOGO_SIZE           (52.0f) /* pixels for half its width */
#define LOGO_TURN_MS        (2600)  /* time for one full turn */

/* The title is a picture (rom:/wordmark.sprite, drawn by
   assets/images/make_wordmark.py): the name in 80s-style letters with "64"
   in neon across it. It is big for this console, about 220 KB, so it is in
   memory only while the intro plays, and when memory is short the name is
   written in plain text with the accent line instead. */
#define WORDMARK_Y          (258)
#define WORDMARK_MIN_FREE   (700 * 1024)

#define TITLE_HEIGHT        (40)
#define TITLE_Y             (272)
#define BAR_Y               (TITLE_Y + TITLE_HEIGHT + 8)
#define BAR_HEIGHT          (4)
#define BAR_WIDTH           (240)

/* How power-on is told from RESET. The console can't tell us: the cart's
   own start-up program always reports a reset. So a mark is left in a small
   piece of the cart's memory (the last 8 bytes of its 64DD sector buffer,
   unused unless a 64DD game runs). The cart keeps its memory through RESET
   and loses it when the power goes, so finding the mark means RESET.
   While the USB cable powers the cart the mark also survives the power
   switch; unplug the cable to see the intro again. */
#define MARK_ADDRESS        (0x1FFE28F8UL)  /* SC64_BUFFERS->DD_SECTOR[248], see flashcart/sc64/sc64_ll.h */
#define MARK_A              (0x53533634)    /* "SS64" */
#define MARK_B              (0x494E5452)    /* "INTR" */

static bool playing = false;
static bool power_on = true;    /* this start was after the power switch (intro_begin) */
static bool started = false;
static uint64_t started_at;
static menu_mode_t after;       /* the screen to open when the intro ends */
static waveform_t *tune = NULL;
static bool tune_open = false;
static sprite_t *wordmark = NULL;
static bool show_logo = true;       /* options `intro_logo` and `intro_title`, read when the intro starts */
static bool show_title = true;

/* Vee draws the title in (when the assistant is on): she flies across it
   from the left with the title showing behind her and sparkles trailing,
   then loops away off the right edge, low, toward her perch on the Games
   tab, where she says hello as usual. */
#define FAIRY_FROM_MS   (350)
#define FAIRY_ACROSS_MS (1300)
#define FAIRY_AWAY_MS   (800)
#define FAIRY_LOOP_SHARE (0.55f)    /* of the way away: the loop */
#define FAIRY_LOOP_R    (34.0f)
#define FAIRY_SIZE      (46)
#define FAIRY_WAVE      (18.0f)     /* up and down as she goes */
#define FAIRY_EXIT_Y    (380.0f)    /* where she leaves the screen: about her perch's height */
#define TRAIL_POINTS    (28)
#define TRAIL_EVERY_MS  (35)
#define TRAIL_LIFE_MS   (700)
static bool with_fairy = false;
static struct { float x, y; int at; } trail[TRAIL_POINTS];
static int trail_next = 0;
static int trail_last_at = -1000;
static bool fairy_left = false;     /* she flew off: the welcome follows on the Games tab */

static int total_ms = INTRO_SHORTEST_MS;
static bool fading = false;         /* the fade-in is waiting to start, or running */
static bool fade_running = false;
static uint64_t fade_asked_at;
static uint64_t fade_from;

static void music_volume (float volume) {
    mixer_ch_set_vol(SOUND_BGM_CHANNEL, volume, volume);
}

static void finish (menu_t *menu) {
    if (tune_open) {
        mixer_ch_stop(INTRO_CHANNEL);
        theme_pack_intro_free();
        tune = NULL;
        tune_open = false;
    }
    intro_logo_close();
    if (with_fairy) {
        assistant_portrait_free();
        with_fairy = false;
        fairy_left = true;
    }
    if (wordmark) {
        sprite_free(wordmark);
        wordmark = NULL;
    }
    playing = false;
    menu->next_mode = after;
}

/* Ask for the fade-in. It starts on the first frame the next screen draws. */
static void fade_ask (void) {
    if (options_get(OPTION_FADE) == FADE_OFF) {
        music_volume(volume_music());
        return;
    }
    music_volume(0.0f);
    fading = true;
    fade_running = false;
    fade_asked_at = get_ticks_ms();
}

/* How far the fade-in has got, 0 to 1. */
static float fade_progress (void) {
    int length = (options_get(OPTION_FADE) == FADE_1_SECOND) ? FADE_FAST_MS : FADE_MS;
    float t = (float) (get_ticks_ms() - fade_from) / length;
    return (t > 1.0f) ? 1.0f : t;
}

/* The picture's part of the fade. With the covers to deal in it takes a
   second whatever the setting (the music still rises over the chosen time):
   the fade's darkening and the deal together are too much for one frame, so
   the deal waits for the picture to be up (cover_deal.h). */
static float picture_progress (void) {
    if (!cover_deal_ahead()) {
        return fade_progress();
    }
    float t = (float) (get_ticks_ms() - fade_from) / FADE_FAST_MS;
    return (t > 1.0f) ? 1.0f : t;
}

/* True if the cart has been powered since the menu last started. Only the
   SummerCart64 has this memory (it is also the only cart with diagnostic
   data, which is how it is recognised); on other carts the answer is "no". */
static bool cart_stayed_on (void) {
    if (!flashcart_has_feature(FLASHCART_FEATURE_DIAGNOSTIC_DATA)) {
        return false;
    }
    uint32_t a = io_read(MARK_ADDRESS);
    uint32_t b = io_read(MARK_ADDRESS + 4);
    io_write(MARK_ADDRESS, MARK_A);
    io_write(MARK_ADDRESS + 4, MARK_B);
    debugf("intro: mark read %08lX %08lX, written back as %08lX %08lX\n",
        (unsigned long) a, (unsigned long) b,
        (unsigned long) io_read(MARK_ADDRESS), (unsigned long) io_read(MARK_ADDRESS + 4));
    return (a == MARK_A) && (b == MARK_B);
}

bool intro_power_on (void) {
    return power_on;
}

bool intro_fairy_flew (void) {
    return fairy_left;
}

bool intro_fairy_handoff (void) {
    if (!fairy_left || fading) {
        return false;
    }
    fairy_left = false;
    debugf("intro: Vee's welcome follows now\n");
    return true;
}

void intro_begin (menu_t *menu) {
    bool cold = !cart_stayed_on();
    power_on = cold;
    int choice = options_get(OPTION_INTRO);
    bool wanted = !safe_mode_active() && ((choice == INTRO_BOTH) || ((choice == INTRO_ON) && cold));

    debugf("intro: %s start, setting %d, %s\n", cold ? "power-on" : "reset", choice, wanted ? "playing" : "skipped");

    if (!wanted) {
        fade_ask();
        return;
    }

    after = menu->next_mode;
    menu->next_mode = MENU_MODE_STARTUP;    /* stay on the startup screen while the intro runs */
    playing = true;
    started = false;
    show_logo = (options_get(OPTION_INTRO_LOGO) != INTRO_LOGO_OFF);
    if (show_logo) {
        intro_logo_open((intro_logo_t) options_get(OPTION_INTRO_LOGO));
    }

    int title = options_get(OPTION_INTRO_TITLE);
    show_title = (title != INTRO_TITLE_OFF);
    if (title == INTRO_TITLE_PICTURE) {
        heap_stats_t heap;
        sys_get_heap_stats(&heap);
        if ((heap.total - heap.used) >= WORDMARK_MIN_FREE) {
            wordmark = sprite_load("rom:/wordmark.sprite");
        } else {
            memory_note_cut(MEMORY_CUT_INTRO_PICTURE);
        }
    }
    with_fairy = show_title && options_get(OPTION_ASSISTANT) != ASSISTANT_OFF;
    trail_next = 0;
    trail_last_at = -1000;
    for (int i = 0; i < TRAIL_POINTS; i++) {
        trail[i].at = -100000;
    }
    debugf("intro: logo %s, title %s, fairy %s\n", show_logo ? "on" : "off",
        !show_title ? "off" : wordmark ? "picture" : (title == INTRO_TITLE_TEXT) ? "as text" : "as text (memory is short)",
        with_fairy ? "draws it" : "off");
    music_volume(0.0f);
}

menu_mode_t intro_hold (menu_t *menu) {
    menu_mode_t was;
    if (playing) {
        was = after;
        after = MENU_MODE_STARTUP;
    } else {
        was = menu->next_mode;
        menu->next_mode = MENU_MODE_STARTUP;
    }
    return was;
}

void intro_fade_wait (void) {
    if (fading) {
        fade_running = false;
        fade_asked_at = get_ticks_ms();
        music_volume(0.0f);
    }
}

const char *intro_choice_name (int choice) {
    switch (choice) {
        case INTRO_OFF: return "Off";
        case INTRO_BOTH: return "Both";
        default: return "On";
    }
}

const char *intro_title_name (int choice) {
    switch (choice) {
        case INTRO_TITLE_TEXT: return "Text";
        case INTRO_TITLE_OFF: return "Off";
        default: return "Picture";
    }
}

const char *intro_fade_name (int choice) {
    switch (choice) {
        case FADE_OFF: return "Off";
        case FADE_1_SECOND: return "1 Second";
        default: return "2 Seconds";
    }
}

void intro_poll (void) {
    if (!fading) {
        return;
    }
    if (!fade_running) {
        if ((get_ticks_ms() - fade_asked_at) < FADE_WAIT_MS) {
            return;
        }
        fade_running = true;
        fade_from = get_ticks_ms();
    }
    float t = fade_progress();
    music_volume(volume_music() * t);
    if (t >= 1.0f) {
        fading = false;
    }
}

int intro_fade_dark (void) {
    if (!fading) {
        return 0;
    }
    if (!fade_running) {
        fade_running = true;
        fade_from = get_ticks_ms();
    }
    /* The picture brightens quickly at first and then more slowly, so the
       menu can be read long before the fade is over. */
    float left = 1.0f - picture_progress();
    int dark = (int) (left * left * 255.0f);
    return (dark < 0) ? 0 : ((dark > 255) ? 255 : dark);
}

void intro_fade_rect (int x0, int y0, int x1, int y1) {
    int dark = intro_fade_dark();
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > DISPLAY_WIDTH) x1 = DISPLAY_WIDTH;
    if (y1 > DISPLAY_HEIGHT) y1 = DISPLAY_HEIGHT;
    if (dark <= 0 || x1 <= x0 || y1 <= y0) {
        return;
    }
    rdpq_mode_push();
        if (dark >= 255) {
            rdpq_set_mode_fill(RGBA32(0x00, 0x00, 0x00, 0xFF));
        } else {
            rdpq_set_mode_standard();
            rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, dark));
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        }
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

void intro_fade_draw (void) {
    intro_fade_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

/* Where Vee is at time t, for a title from x0 to x1 around line y_mid.
   @return false while she isn't on the screen yet. */
static bool fairy_at (int t, float x0, float x1, float y_mid, float *x, float *y) {
    float u = (float) (t - FAIRY_FROM_MS) / FAIRY_ACROSS_MS;
    if (u < 0.0f) {
        return false;
    }
    float end_x = x1 + 30.0f;
    if (u <= 1.0f) {
        float e = u * u * (3.0f - 2.0f * u);     /* in and out gently */
        *x = -40.0f + (end_x + 40.0f) * e;
        *y = y_mid + FAIRY_WAVE * sinf(u * 3.0f * (float) M_PI);
        return true;
    }
    float v = (float) (t - FAIRY_FROM_MS - FAIRY_ACROSS_MS) / FAIRY_AWAY_MS;
    if (v > 1.0f) {
        return false;
    }
    /* A loop-the-loop just past the title's end, then away, low, off the right edge. */
    if (v < FAIRY_LOOP_SHARE) {
        float a = 2.0f * (float) M_PI * (v / FAIRY_LOOP_SHARE);
        *x = end_x + FAIRY_LOOP_R * sinf(a);
        *y = y_mid - FAIRY_LOOP_R + FAIRY_LOOP_R * cosf(a);
        return true;
    }
    float w = (v - FAIRY_LOOP_SHARE) / (1.0f - FAIRY_LOOP_SHARE);
    *x = end_x + (DISPLAY_WIDTH + 60.0f - end_x) * w * w;
    *y = y_mid + (FAIRY_EXIT_Y - y_mid) * w * (2.0f - w);
    return true;
}

/* The sparkles she leaves: added as she goes, each fading and sinking. */
static void trail_draw (int t, bool add, float x, float y) {
    if (add && t - trail_last_at >= TRAIL_EVERY_MS) {
        trail_last_at = t;
        int n = trail_next;
        trail[n].x = x - 10.0f + (float) ((n * 37) % 21);    /* scattered a little round her */
        trail[n].y = y - 8.0f + (float) ((n * 53) % 17);
        trail[n].at = t;
        trail_next = (n + 1) % TRAIL_POINTS;
    }
    for (int i = 0; i < TRAIL_POINTS; i++) {
        int age = t - trail[i].at;
        if (age < 0 || age >= TRAIL_LIFE_MS) {
            continue;
        }
        float k = 1.0f - (float) age / TRAIL_LIFE_MS;
        assistant_star_draw(trail[i].x, trail[i].y + age * 0.03f, 4.0f + 10.0f * k, (uint8_t) (255 * k), (i & 1) != 0);
    }
}

/* A glint: once the title is in, a slanted band of light sweeps across it
   once. The picture is drawn again over itself in small cells, each blended
   toward white by how near the band it is (the chip's adding mode overflows,
   so it is a blend), so the light shows only where the letters are. */
#define GLINT_MS        (650)
#define GLINT_AFTER_MS  (700)       /* from the start, when Vee isn't drawing the title */
#define GLINT_HALF      (30.0f)     /* half the band's width */
#define GLINT_SLANT     (0.45f)     /* across per line down: it leans like a shine on glass */
#define GLINT_STRENGTH  (180)       /* at its middle, of 255 */
#define GLINT_CELL_W    (4)
#define GLINT_CELL_H    (12)

static int glint_start (void) {
    return with_fairy ? FAIRY_FROM_MS + FAIRY_ACROSS_MS + 150 : GLINT_AFTER_MS;
}

/* Where the band's middle is (across the title's middle line), or false when there is none. */
static bool glint_at (int t, float x0, float x1, float height, float *band) {
    float u = (float) (t - glint_start()) / GLINT_MS;
    if (u <= 0.0f || u >= 1.0f) {
        return false;
    }
    u = u * u * (3.0f - 2.0f * u);
    float lean = GLINT_SLANT * height / 2 + GLINT_HALF;
    *band = (x0 - lean) + (x1 - x0 + 2 * lean) * u;
    return true;
}

static void glint_picture (int t, sprite_t *picture, int x, int y) {
    float band;
    if (!glint_at(t, x, x + picture->width, picture->height, &band)) {
        return;
    }
    float mid_y = picture->height / 2.0f;
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));    /* white, as see-through as the letters */
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        for (int cy = 0; cy < picture->height; cy += GLINT_CELL_H) {
            int ch = (cy + GLINT_CELL_H <= picture->height) ? GLINT_CELL_H : picture->height - cy;
            float shift = (mid_y - (cy + ch / 2.0f)) * GLINT_SLANT;     /* higher lines further right */
            int from = (int) (band + shift - GLINT_HALF - x) & ~(GLINT_CELL_W - 1);
            for (int cx = (from < 0) ? 0 : from; cx < picture->width && cx < band + shift + GLINT_HALF - x; cx += GLINT_CELL_W) {
                float d = fabsf((x + cx + GLINT_CELL_W / 2.0f) - (band + shift)) / GLINT_HALF;
                if (d >= 1.0f) {
                    continue;
                }
                float k = (1.0f - d) * (1.0f - d);
                int cw = (cx + GLINT_CELL_W <= picture->width) ? GLINT_CELL_W : picture->width - cx;
                rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, (uint8_t) (GLINT_STRENGTH * k)));
                rdpq_sprite_blit(picture, x + cx, y + cy, &(rdpq_blitparms_t) { .s0 = cx, .t0 = cy, .width = cw, .height = ch });
            }
        }
    rdpq_mode_pop();
}

bool intro_display (menu_t *menu, surface_t *display) {
    if (!playing) {
        return false;
    }

    /* The first frame builds the theme background, which takes a moment, so
       it is drawn fully black and the clock and the tune start after it. */
    int t = started ? (int) (get_ticks_ms() - started_at) : 0;

    bool skip = menu->actions.enter || menu->actions.back || menu->actions.options || menu->actions.settings;
    bool over = skip || (t >= total_ms);

    /* How dark the picture is: 1 at both ends, 0 in the middle. */
    float dark = 0.0f;
    if (over) {
        dark = 1.0f;
    } else if (t < INTRO_FADE_IN_MS) {
        dark = 1.0f - ((float) t / INTRO_FADE_IN_MS);
    } else if (t > (total_ms - INTRO_FADE_OUT_MS)) {
        dark = 1.0f - ((float) (total_ms - t) / INTRO_FADE_OUT_MS);
    }

    /* The line grows out from the middle, quickly at first and then slowing. */
    float grow = (float) (t - INTRO_BAR_START_MS) / INTRO_BAR_GROW_MS;
    if (grow < 0.0f) grow = 0.0f;
    if (grow > 1.0f) grow = 1.0f;
    grow = 1.0f - ((1.0f - grow) * (1.0f - grow));
    int half_bar = (int) ((BAR_WIDTH / 2) * grow);

    rdpq_attach(display, NULL);

    ui_components_background_draw();

    /* With only one of the two, it sits in the middle of the screen. */
    if (show_logo) {
        int logo_y = show_title ? LOGO_Y : DISPLAY_CENTER_Y;
        intro_logo_draw(DISPLAY_CENTER_X, logo_y, LOGO_SIZE, ((t % LOGO_TURN_MS) * 360.0f) / LOGO_TURN_MS);
    }

    /* Vee and how much of the title she has drawn so far. */
    float title_x0 = wordmark ? DISPLAY_CENTER_X - (wordmark->width / 2) : DISPLAY_CENTER_X - 160;
    float title_x1 = wordmark ? DISPLAY_CENTER_X + (wordmark->width / 2) : DISPLAY_CENTER_X + 160;
    float title_mid = wordmark ? (show_logo ? WORDMARK_Y : DISPLAY_CENTER_Y - (wordmark->height / 2)) + wordmark->height / 2
        : (show_logo ? TITLE_Y + TITLE_HEIGHT / 2 : DISPLAY_CENTER_Y);
    float fairy_x = 0.0f, fairy_y = 0.0f;
    bool fairy_shown = with_fairy && !over && fairy_at(t, title_x0, title_x1, title_mid, &fairy_x, &fairy_y);
    if (with_fairy && !over && t < FAIRY_FROM_MS + FAIRY_ACROSS_MS) {
        int reveal = fairy_shown ? (int) (fairy_x - 4.0f) : 0;     /* the title shows up to just behind her */
        rdpq_set_scissor(0, 0, (reveal < 0) ? 0 : (reveal > DISPLAY_WIDTH ? DISPLAY_WIDTH : reveal), DISPLAY_HEIGHT);
    }

    if (show_title && wordmark) {
        int y = show_logo ? WORDMARK_Y : DISPLAY_CENTER_Y - (wordmark->height / 2);
        rdpq_mode_push();
            rdpq_set_mode_standard();
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_sprite_blit(wordmark, DISPLAY_CENTER_X - (wordmark->width / 2), y, NULL);
        rdpq_mode_pop();
        glint_picture(t, wordmark, DISPLAY_CENTER_X - (wordmark->width / 2), y);
    } else if (show_title) {
        /* The name and its line, moved up together when there is no logo. */
        int shift = show_logo ? 0 : (DISPLAY_CENTER_Y - ((BAR_Y + BAR_HEIGHT - TITLE_Y) / 2)) - TITLE_Y;
        rdpq_text_printf(&(rdpq_textparms_t) {
            .width = DISPLAY_WIDTH,
            .height = TITLE_HEIGHT,
            .align = ALIGN_CENTER,
            .valign = VALIGN_CENTER,
            .style_id = STL_DEFAULT,
        }, FNT_TITLE, 0, TITLE_Y + shift, "%s", MENU_DISPLAY_NAME);
        float band;
        if (glint_at(t, title_x0, title_x1, TITLE_HEIGHT, &band)) {
            /* The name again in white, only inside a narrow band: the glint on plain letters. */
            rdpq_set_scissor((int) (band - GLINT_HALF / 2), 0, (int) (band + GLINT_HALF / 2), DISPLAY_HEIGHT);
            rdpq_text_printf(&(rdpq_textparms_t) {
                .width = DISPLAY_WIDTH,
                .height = TITLE_HEIGHT,
                .align = ALIGN_CENTER,
                .valign = VALIGN_CENTER,
                .style_id = STL_WHITE,
            }, FNT_TITLE, 0, TITLE_Y + shift, "%s", MENU_DISPLAY_NAME);
            rdpq_set_scissor(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        }
    
        if (half_bar > 0) {
            rdpq_mode_push();
                rdpq_set_mode_fill(theme_get()->accent);
                rdpq_fill_rectangle(DISPLAY_CENTER_X - half_bar, BAR_Y + shift, DISPLAY_CENTER_X + half_bar, BAR_Y + shift + BAR_HEIGHT);
            rdpq_mode_pop();
        }
    }
    rdpq_set_scissor(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    if (with_fairy && !over) {
        trail_draw(t, fairy_shown, fairy_x, fairy_y);
        if (fairy_shown) {
            assistant_fairy_draw(fairy_x, fairy_y, FAIRY_SIZE, 255);
        }
    }

    if (dark > 0.0f) {
        rdpq_mode_push();
            if (dark >= 1.0f) {
                rdpq_set_mode_fill(RGBA32(0x00, 0x00, 0x00, 0xFF));
            } else {
                rdpq_set_mode_standard();
                rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, (int) (dark * 255.0f)));
                rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
                rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            }
            rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        rdpq_mode_pop();
    }

    rdpq_detach_show();

    if (over) {
        finish(menu);
        fade_ask();
    } else if (!started) {
        started = true;
        /* The tune is opened even when it won't be heard, to learn its length. */
        uint64_t opening = get_ticks_ms();
        tune = theme_pack_intro();     /* a card tune is read into memory whole: streamed, it crunched */
        debugf("intro: tune ready in %d ms\n", (int) (get_ticks_ms() - opening));
        started_at = get_ticks_ms();    /* after reading it (a second for a long card tune), so the picture keeps time with the tune */
        tune_open = true;
        if ((tune->len > 0) && (tune->frequency > 0.0f)) {
            total_ms = (int) ((tune->len * 1000.0f) / tune->frequency);
        }
        if (total_ms < INTRO_SHORTEST_MS) total_ms = INTRO_SHORTEST_MS;
        if (total_ms > INTRO_LONGEST_MS) total_ms = INTRO_LONGEST_MS;
        debugf("intro: tune lasts %d ms (%d samples at %d Hz, loop %d)\n", total_ms,
            (int) tune->len, (int) tune->frequency, (int) tune->loop_len);
        if (menu->settings.soundfx_enabled) {
            mixer_ch_set_vol(INTRO_CHANNEL, volume_effects(), volume_effects());
            mixer_ch_play(INTRO_CHANNEL, tune);
        }
    }

    return true;
}
