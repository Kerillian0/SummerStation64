#include <libdragon.h>

#include "menu_options.h"
#include "sound.h"
#include "volume.h"

#define MUSIC_BASE      (0.1f)  /* the stock menu's levels, at the middle of each slider */
#define EFFECTS_BASE    (0.5f)

static float level (option_t option, float base) {
    int v = options_get(option);
    v = (v < 0) ? 0 : ((v > VOLUME_STEPS) ? VOLUME_STEPS : v);
    return base * v / (VOLUME_STEPS / 2);
}

/* +3 dB steps as factors: 10^(dB / 20). */
static const float BOOST[VOLUME_BOOST_STEPS] = { 1.0f, 1.4125f, 1.9953f, 2.8184f, 3.9811f };
static const char *const BOOST_NAMES[VOLUME_BOOST_STEPS] = { "Off", "+3 dB", "+6 dB", "+9 dB", "+12 dB" };

const char *volume_boost_name (int i) {
    return (i >= 0 && i < VOLUME_BOOST_STEPS) ? BOOST_NAMES[i] : "";
}

float volume_music (void) {
    int b = options_get(OPTION_MUSIC_BOOST);
    b = (b < 0) ? 0 : ((b >= VOLUME_BOOST_STEPS) ? VOLUME_BOOST_STEPS - 1 : b);
    float v = level(OPTION_MUSIC_VOLUME, MUSIC_BASE) * BOOST[b];
    return (v > 1.0f) ? 1.0f : v;
}

float volume_effects (void) {
    return level(OPTION_SOUND_VOLUME, EFFECTS_BASE);
}

void volume_apply (void) {
    float m = volume_music(), e = volume_effects();
    mixer_ch_set_vol(SOUND_BGM_CHANNEL, m, m);
    mixer_ch_set_vol(SOUND_SFX_CHANNEL, e, e);
}
