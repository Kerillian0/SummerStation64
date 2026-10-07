/**
 * @file intro.c
 * @brief The short intro shown when the console is switched on.
 */

#include <libdragon.h>
#include "flashcart/flashcart.h"
#include "fonts.h"
#include "intro.h"
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
#define INTRO_VOLUME        (0.5f)

/* The fade-in (option `fade`): when the first screen appears, with or
   without the intro before it, the picture comes up from black and the
   background music (if the player has it on) rises from silence, together. */
#define FADE_MS             (2000)
#define FADE_FAST_MS        (1000)
#define FADE_WAIT_MS        (1000)  /* if no screen has drawn the fade by then, the music starts rising anyway */
#define MUSIC_VOLUME        (0.1f)  /* the level sound.c plays it at */

#define TITLE_HEIGHT        (40)
#define TITLE_Y             (DISPLAY_CENTER_Y - TITLE_HEIGHT)
#define BAR_Y               (DISPLAY_CENTER_Y + 10)
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
static bool started = false;
static uint64_t started_at;
static menu_mode_t after;       /* the screen to open when the intro ends */
static wav64_t tune;
static bool tune_open = false;
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
        wav64_close(&tune);
        tune_open = false;
    }
    playing = false;
    menu->next_mode = after;
}

/* Ask for the fade-in. It starts on the first frame the next screen draws. */
static void fade_ask (void) {
    if (options_get(OPTION_FADE) == FADE_OFF) {
        music_volume(MUSIC_VOLUME);
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

void intro_begin (menu_t *menu) {
    bool cold = !cart_stayed_on();
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
    music_volume(0.0f);
}

const char *intro_choice_name (int choice) {
    switch (choice) {
        case INTRO_OFF: return "Off";
        case INTRO_BOTH: return "Both";
        default: return "On";
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
    music_volume(MUSIC_VOLUME * t);
    if (t >= 1.0f) {
        fading = false;
    }
}

void intro_fade_draw (void) {
    if (!fading) {
        return;
    }
    if (!fade_running) {
        fade_running = true;
        fade_from = get_ticks_ms();
    }
    /* The picture brightens quickly at first and then more slowly, so the
       menu can be read long before the fade is over. */
    float left = 1.0f - fade_progress();
    int dark = (int) (left * left * 255.0f);
    if (dark <= 0) {
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
        rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
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

    rdpq_text_printf(&(rdpq_textparms_t) {
        .width = DISPLAY_WIDTH,
        .height = TITLE_HEIGHT,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .style_id = STL_DEFAULT,
    }, FNT_TITLE, 0, TITLE_Y, "%s", MENU_DISPLAY_NAME);

    if (half_bar > 0) {
        rdpq_mode_push();
            rdpq_set_mode_fill(theme_get()->accent);
            rdpq_fill_rectangle(DISPLAY_CENTER_X - half_bar, BAR_Y, DISPLAY_CENTER_X + half_bar, BAR_Y + BAR_HEIGHT);
        rdpq_mode_pop();
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
        started_at = get_ticks_ms();
        /* The tune is opened even when it won't be heard, to learn its length. */
        wav64_open(&tune, "rom:/intro.wav64");
        tune_open = true;
        if ((tune.wave.len > 0) && (tune.wave.frequency > 0.0f)) {
            total_ms = (int) ((tune.wave.len * 1000.0f) / tune.wave.frequency);
        }
        if (total_ms < INTRO_SHORTEST_MS) total_ms = INTRO_SHORTEST_MS;
        if (total_ms > INTRO_LONGEST_MS) total_ms = INTRO_LONGEST_MS;
        debugf("intro: tune lasts %d ms\n", total_ms);
        if (menu->settings.soundfx_enabled) {
            mixer_ch_set_vol(INTRO_CHANNEL, INTRO_VOLUME, INTRO_VOLUME);
            wav64_play(&tune, INTRO_CHANNEL);
        }
    }

    return true;
}
