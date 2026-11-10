#include <libdragon.h>

#include "animations.h"
#include "menu_options.h"

static uint64_t last_press_ms = 0;
static int last_direction = 1;

const char *animations_name (int choice) {
    static const char *names[ANIMATIONS_COUNT] = { "Snappy", "Showy" };
    return (choice >= 0 && choice < ANIMATIONS_COUNT) ? names[choice] : "";
}

bool animations_showy (void) {
    return options_get(OPTION_ANIMATIONS) == ANIMATIONS_SHOWY;
}

void animations_note_press (void) {
    last_press_ms = get_ticks_ms();
}

float animations_progress (uint64_t since, int ms) {
    if (!animations_showy() || last_press_ms > since || ms <= 0) {
        return 1.0f;
    }
    float t = (float) (get_ticks_ms() - since) / ms;
    return (t > 1.0f) ? 1.0f : ((t < 0.0f) ? 0.0f : t);
}

float animations_ease_out (float t) {
    float rest = 1.0f - t;
    return 1.0f - rest * rest * rest;
}

float animations_ease_back (float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

void animations_note_direction (int direction) {
    if (direction != 0) {
        last_direction = (direction > 0) ? 1 : -1;
    }
}

int animations_direction (void) {
    return last_direction;
}
