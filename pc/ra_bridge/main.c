/**
 * @file main.c
 * @brief ra_bridge, the RetroAchievements experiment's PC program, step 2b:
 * the same test as scripts/ra_pipe_test.py, as a Windows program. It asks the
 * game's hook for Mario's coins, stars, lives and health in Super Mario 64
 * (USA), goes through every frame in order, prints each change with its
 * frame and counts any frame missed. Step 2c puts rcheevos in its place.
 *
 *     ra_bridge.exe              (finds the SummerCart64 by itself)
 *     ra_bridge.exe --port COM3
 *
 * Built in the dev container: sh pc/ra_bridge/build.sh -> output/ra_bridge.exe
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "hook_ring.h"
#include "sc64_link.h"

static volatile LONG stop = 0;

static BOOL WINAPI on_ctrl_c (DWORD type) {
    (void) type;
    InterlockedExchange(&stop, 1);
    return TRUE;
}

typedef struct { int coins, stars, lives, health; bool known; } mario_t;

static int s16 (const uint8_t *p) {
    return (int16_t) ((p[0] << 8) | p[1]);
}

static void on_frame (uint32_t frame, const uint8_t *data, uint32_t length, void *context) {
    mario_t *last = context;
    if (length < 8) {
        return;
    }
    mario_t now = { s16(data), s16(data + 2), (int8_t) data[5], s16(data + 6) >> 8, true };
    if (last->known && memcmp(&now, last, sizeof(now)) != 0) {
        printf("\rframe %8lu:", (unsigned long) frame);
        if (now.coins != last->coins) printf(" coins %d -> %d", last->coins, now.coins);
        if (now.stars != last->stars) printf(" stars %d -> %d", last->stars, now.stars);
        if (now.lives != last->lives) printf(" lives %d -> %d", last->lives, now.lives);
        if (now.health != last->health) printf(" health %d -> %d", last->health, now.health);
        printf("%40s\n", "");
    }
    *last = now;
}

int main (int argc, char **argv) {
    const char *port = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = argv[++i];
        }
    }
    SetConsoleCtrlHandler(on_ctrl_c, TRUE);

    if (!sc64_open(port)) {
        fprintf(stderr, "Could not reach the SummerCart64: %s\n", sc64_error());
        return 1;
    }
    char ident[5];
    if (!sc64_identify(ident)) {
        fprintf(stderr, "The cart on %s did not identify itself: %s\n", sc64_port(), sc64_error());
        return 1;
    }
    printf("Opened %s, cart says \"%s\" (expected \"SCv2\")\n", sc64_port(), ident);

    hook_range_t ranges[] = {
        { 0x0033B218, 4 },  /* coins (s16), stars (s16) */
        { 0x0033B21C, 4 },  /* (a byte), lives (s8), health (s16: wedges in the high byte) */
    };
    if (!hook_set_list(ranges, 2)) {
        fprintf(stderr, "Could not give the hook its list: %s\n", sc64_error());
        return 1;
    }
    printf("Asked for 2 ranges, 8 bytes a frame. Start Super Mario 64 (USA) if it isn't running yet. Ctrl+C stops.\n\n");

    mario_t mario = { 0 };
    DWORD started = GetTickCount();
    while (!stop) {
        bool running = false;
        if (!hook_poll(on_frame, &mario, &running)) {
            fprintf(stderr, "\nReading the cart failed: %s\n", sc64_error());
            break;
        }
        if (!running) {
            printf("\rNo hook yet (start the game with ra_test = 1).        ");
        } else if (mario.known) {
            double seconds = (GetTickCount() - started) / 1000.0;
            printf("\r%5.1f frames/s checked, %lu missed  |  coins %3d  stars %3d  lives %2d  health %d/8   ",
                   hook_frames_seen() / (seconds > 0.001 ? seconds : 0.001), (unsigned long) hook_frames_missed(),
                   mario.coins, mario.stars, mario.lives, mario.health);
        }
        fflush(stdout);
        Sleep(100);
    }
    printf("\nStopped: %lu frames checked, %lu missed.\n", (unsigned long) hook_frames_seen(), (unsigned long) hook_frames_missed());
    sc64_close();
    return 0;
}
