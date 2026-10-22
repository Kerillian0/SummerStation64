/**
 * @file box_motion.c
 * @brief How the 3D Box moves: the hover, and turning it by hand.
 */

#include <math.h>
#include <libdragon.h>

#include "box_motion.h"
#include "menu_features.h"
#include "menu_options.h"

/* The hover: how far and how slowly. Periods that don't divide into each
   other, so the movement never looks like a loop. */
#define FLOAT_PX            (4.0f)      /* up and down, either way */
#define FLOAT_PERIOD_MS     (2800.0f)
#define SWAY_YAW            (6.0f)      /* degrees either way */
#define SWAY_YAW_PERIOD_MS  (5000.0f)
#define SWAY_PITCH          (2.0f)
#define SWAY_PITCH_PERIOD_MS (3700.0f)

/* Turning by hand: degrees a second with the stick pushed all the way,
   and how far it can go in all (the resting angle included). */
#define TURN_SPEED          (150.0f)
#define YAW_LIMIT           (75.0f)
#define PITCH_LOW           (-40.0f)
#define PITCH_HIGH          (50.0f)
#define STICK_FULL          (70)        /* a stick pushed this far counts as all the way */
#define STICK_DEAD          (8)

/* How fast the box settles back, and the hover comes back, after B is let
   go: the share of the way covered in one second. */
#define SETTLE_RATE         (8.0f)

static float turn_yaw = 0.0f;       /* the turn by hand, added to the resting angles */
static float turn_pitch = 0.0f;
static float hover = 1.0f;          /* 1: hovering fully, 0: still (while B is held) */
static bool holding = false;
static uint64_t last_ms = 0;
static uint64_t shown_ms = 0;
static float rest_yaw = 0.0f, rest_pitch = 0.0f;    /* the last resting angles, for the limits */

const char *box_motion_hover_name (int choice) {
    static const char *names[BOX_HOVER_COUNT] = { "Off", "Float", "Sway", "Float and Sway" };
    return (choice >= 0 && choice < BOX_HOVER_COUNT) ? names[choice] : "";
}

void box_motion_shown (void) {
    shown_ms = get_ticks_ms();
}

void box_motion_reset (void) {
    turn_yaw = turn_pitch = 0.0f;
    hover = 1.0f;
}

static float stick_amount (int value) {
    if (value > -STICK_DEAD && value < STICK_DEAD) {
        return 0.0f;
    }
    float amount = (float) value / STICK_FULL;
    return (amount > 1.0f) ? 1.0f : ((amount < -1.0f) ? -1.0f : amount);
}

static float clamp (float value, float low, float high) {
    return (value < low) ? low : ((value > high) ? high : value);
}

bool box_motion_process (menu_t *menu) {
    uint64_t now = get_ticks_ms();
    float dt = (last_ms == 0 || now - last_ms > 200) ? 0.0f : (now - last_ms) / 1000.0f;
    last_ms = now;

    /* Only a box drawn a moment ago can be turned. */
    bool box_showing = features_enabled(FEATURE_BOX_3D) && (now - shown_ms) < 150;

    holding = false;
    joypad_inputs_t inputs = { 0 };
    if (box_showing) {
        JOYPAD_PORT_FOREACH (port) {
            joypad_inputs_t in = joypad_get_inputs(port);
            if (in.btn.b) {
                inputs = in;
                holding = true;
                break;
            }
        }
    }

    if (holding) {
        float x = stick_amount(inputs.stick_x);
        float y = stick_amount(inputs.stick_y);
        if (inputs.btn.d_right) x = 1.0f;
        if (inputs.btn.d_left) x = -1.0f;
        if (inputs.btn.d_up) y = 1.0f;
        if (inputs.btn.d_down) y = -1.0f;
        /* Right turns the front to the right, up tips the front up. */
        turn_yaw = clamp(turn_yaw + x * TURN_SPEED * dt, -YAW_LIMIT - rest_yaw, YAW_LIMIT - rest_yaw);
        turn_pitch = clamp(turn_pitch - y * TURN_SPEED * dt, PITCH_LOW - rest_pitch, PITCH_HIGH - rest_pitch);
        hover = clamp(hover - SETTLE_RATE * dt, 0.0f, 1.0f);

        /* The row stays where it is meanwhile, and B is not "back". */
        menu->actions.go_up = false;
        menu->actions.go_down = false;
        menu->actions.go_left = false;
        menu->actions.go_right = false;
        menu->actions.go_fast = false;
        menu->actions.back = false;
    } else {
        float k = clamp(SETTLE_RATE * dt, 0.0f, 1.0f);
        turn_yaw -= turn_yaw * k;
        turn_pitch -= turn_pitch * k;
        hover = clamp(hover + SETTLE_RATE * dt * 0.25f, 0.0f, 1.0f);
    }
    return holding;
}

float box_motion_side_lift (int index, float weight) {
    int choice = options_get(OPTION_BOX_HOVER);
    if (choice != BOX_HOVER_FLOAT && choice != BOX_HOVER_BOTH) {
        return 0.0f;
    }
    /* A different starting point and a slightly different pace for each
       cover, so the row doesn't bob together. */
    float phase = (index % 7) * 0.9f;
    float period = FLOAT_PERIOD_MS * (1.0f + (index % 3) * 0.12f);
    float t = (float) get_ticks_ms();
    return weight * FLOAT_PX * sinf(2.0f * (float) M_PI * t / period + phase);
}

void box_motion_pose (float *yaw, float *pitch, float *lift) {
    rest_yaw = *yaw;
    rest_pitch = *pitch;
    *lift = 0.0f;

    int choice = options_get(OPTION_BOX_HOVER);
    float t = (float) get_ticks_ms();
    const float tau = 2.0f * (float) M_PI;
    if (choice == BOX_HOVER_SWAY || choice == BOX_HOVER_BOTH) {
        *yaw += hover * SWAY_YAW * sinf(tau * t / SWAY_YAW_PERIOD_MS);
        *pitch += hover * SWAY_PITCH * sinf(tau * t / SWAY_PITCH_PERIOD_MS);
    }
    if (choice == BOX_HOVER_FLOAT || choice == BOX_HOVER_BOTH) {
        /* Smooth, between pixels: the box is drawn with smoothing, so unlike
           text it doesn't shimmer when it isn't on whole 2-pixel steps. */
        *lift = hover * FLOAT_PX * sinf(tau * t / FLOAT_PERIOD_MS);
    }
    *yaw += turn_yaw;
    *pitch += turn_pitch;
}
