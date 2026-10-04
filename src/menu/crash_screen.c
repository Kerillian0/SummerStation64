#include <libdragon.h>

#include "crash_screen.h"

#define SCREEN_WIDTH    (320)
#define SCREEN_HEIGHT   (240)
#define TEXT_X          (24)
#define TEXT_Y          (32)
#define LINE_HEIGHT     (12)

/* A one-off, blocking controller read that works with interrupts off.
   libdragon provides it but doesn't list it in the public joypad.h. */
joypad_inputs_t joypad_read_n64_inputs (joypad_port_t port);

/* Applies pending video register changes even if the crash interrupted them.
   Also provided by libdragon without a public declaration. */
void vi_write_end_forced (void);

/* Lines are at most 34 characters: the built-in font is 8 pixels wide. */
static const char *const lines[] = {
    "",
    "The menu ran into a problem and",
    "had to stop.",
    "",
    "Turn the console off and on again.",
    "",
    "If it keeps happening, hold Z",
    "while turning the console on to",
    "start in Safe Mode.",
    "",
    "",
    "START: technical details",
};

static bool start_is_down (void) {
    JOYPAD_PORT_FOREACH (port) {
        if (joypad_read_n64_inputs(port).btn.start) {
            return true;
        }
    }
    return false;
}

static void crash_handler (exception_t *ex) {
    static bool entered = false;

    /* If this screen itself fails, fall back to the technical one. */
    if (entered) {
        exception_default_handler(ex);
    }
    entered = true;

    /* Free the menu's framebuffers and put the video output in a known state.
       Same steps as libdragon's own crash screen: finish any half-applied video
       settings, then reset them. Without the reset the menu's 640x480 settings
       stay in place and a 320x240 picture comes out repeated and striped. */
    vi_write_end_forced();
    display_close();
    vi_init();
    vi_reset();

    surface_t screen = surface_alloc(FMT_RGBA16, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!screen.buffer) {
        exception_default_handler(ex);
    }

    uint32_t background = graphics_make_color(0x10, 0x18, 0x30, 0xFF);
    uint32_t heading = graphics_make_color(0xF2, 0xB1, 0x34, 0xFF);
    uint32_t text = graphics_make_color(0xE9, 0xED, 0xF2, 0xFF);

    graphics_fill_screen(&screen, background);
    graphics_set_color(heading, background);
    graphics_draw_text(&screen, TEXT_X, TEXT_Y, "Something went wrong");
    graphics_set_color(text, background);
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        graphics_draw_text(&screen, TEXT_X, TEXT_Y + (i + 1) * LINE_HEIGHT, lines[i]);
    }

    vi_show(&screen);
    vi_wait_vblank();

    /* Wait for a fresh press of START (it may be held down already). */
    bool was_down = true;
    while (true) {
        bool down = start_is_down();
        if (down && !was_down) {
            break;
        }
        was_down = down;
        wait_ms(10); /* leave the controller port alone so RESET keeps working */
    }

    vi_show(NULL);
    vi_wait_vblank();
    surface_free(&screen);

    exception_default_handler(ex);
}

void crash_screen_init (void) {
    register_exception_handler(crash_handler);
}
