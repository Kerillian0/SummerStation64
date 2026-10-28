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

float volume_music (void) {
    return level(OPTION_MUSIC_VOLUME, MUSIC_BASE);
}

float volume_effects (void) {
    return level(OPTION_SOUND_VOLUME, EFFECTS_BASE);
}

void volume_apply (void) {
    float m = volume_music(), e = volume_effects();
    mixer_ch_set_vol(SOUND_BGM_CHANNEL, m, m);
    mixer_ch_set_vol(SOUND_SFX_CHANNEL, e, e);
}
