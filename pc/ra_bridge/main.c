/**
 * @file main.c
 * @brief ra_bridge, the RetroAchievements experiment's PC program, step 2d.
 *
 * Finds the SummerCart64, waits for a game started with the hidden option
 * ra_test = 1, works out the game's RetroAchievements hash from the ROM in
 * the cart, logs in (once; only the token is kept), downloads the game's
 * achievements and checks them against every frame the game's hook copies
 * (the hook also follows the achievements' pointer chains, see watch.h).
 * Each one met is printed as "WOULD UNLOCK": nothing is ever sent to the
 * account (the owner's choice until RetroAchievements approves the bridge).
 *
 *     ra_bridge.exe                  (finds the SummerCart64 by itself)
 *     ra_bridge.exe --prepare E:\    (no N64 needed: puts every game's achievement set
 *                                     and badges on the SD card in the PC's reader,
 *                                     for the menu's own achievements; see prep.h)
 *     ra_bridge.exe --port COM3
 *     ra_bridge.exe --view native    (the console's own byte order instead of
 *                                     RetroAchievements' swapped view; for tests)
 *
 * The downloaded definitions are saved beside the .exe as
 * ra_bridge_game_<id>.txt, to look at.
 *
 * Built in the dev container: sh pc/ra_bridge/build.sh -> output/ra_bridge.exe
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "game.h"
#include "hook_ring.h"
#include "prep.h"
#include "ra_web.h"
#include "sc64_link.h"
#include "watch.h"

static volatile LONG stop = 0;

static BOOL WINAPI on_ctrl_c (DWORD type) {
    (void) type;
    InterlockedExchange(&stop, 1);
    return TRUE;
}

static void save_definitions (const rc_api_fetch_game_data_response_t *game) {
    char path[MAX_PATH + 64];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash = (n > 0) ? strrchr(path, '\\') : NULL;
    if (slash) {
        slash[1] = '\0';
    } else {
        path[0] = '\0';
    }
    char name[64];
    snprintf(name, sizeof(name), "ra_bridge_game_%lu.txt", (unsigned long) game->id);
    strncat(path, name, sizeof(path) - strlen(path) - 1);
    FILE *f = fopen(path, "w");
    if (!f) {
        return;
    }
    fprintf(f, "%s (RetroAchievements game %lu): %lu achievements\n\n", game->title ? game->title : "",
            (unsigned long) game->id, (unsigned long) game->num_achievements);
    for (uint32_t i = 0; i < game->num_achievements; i++) {
        const rc_api_achievement_definition_t *d = &game->achievements[i];
        fprintf(f, "%lu\t%s\t%lu points\t%s\n\t%s\n\t%s\n\n", (unsigned long) d->id,
                d->category == RC_ACHIEVEMENT_CATEGORY_PROMOTED ? "promoted" : "unpromoted", (unsigned long) d->points,
                d->title ? d->title : "", d->description ? d->description : "", d->definition ? d->definition : "");
    }
    fclose(f);
    printf("Definitions saved to %s\n", path);
}

int main (int argc, char **argv) {
    const char *port = NULL;
    const char *prepare = NULL;
    watch_view_t view = WATCH_VIEW_SWAPPED;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "--prepare") == 0 && i + 1 < argc) {
            prepare = argv[++i];
        } else if (strcmp(argv[i], "--view") == 0 && i + 1 < argc) {
            view = (strcmp(argv[++i], "native") == 0) ? WATCH_VIEW_NATIVE : WATCH_VIEW_SWAPPED;
        }
    }
    SetConsoleCtrlHandler(on_ctrl_c, TRUE);
    SetConsoleOutputCP(CP_UTF8);

    if (prepare) {
        if (!ra_web_login()) {
            fprintf(stderr, "Could not log in to RetroAchievements: %s\n", ra_web_error());
            return 1;
        }
        return prep_card(prepare) ? 0 : 1;
    }

    if (!sc64_open(port)) {
        fprintf(stderr, "Could not reach the SummerCart64: %s\n", sc64_error());
        return 1;
    }
    char ident[5];
    if (!sc64_identify(ident)) {
        fprintf(stderr, "The cart on %s did not identify itself: %s\n", sc64_port(), sc64_error());
        return 1;
    }
    printf("Opened %s, cart says \"%s\"\n", sc64_port(), ident);

    if (!ra_web_login()) {
        fprintf(stderr, "Could not log in to RetroAchievements: %s\n", ra_web_error());
        return 1;
    }

    game_note_t note;
    printf("Waiting for a game started with ra_test = 1...\n");
    while (!stop && !game_read_note(&note)) {
        Sleep(500);
    }
    if (stop) {
        return 0;
    }
    printf("Game: %s  (code %s, %lu bytes)\n", note.path, note.code, (unsigned long) note.rom_size);
    char hash[33];
    if (!game_hash(&note, hash)) {
        fprintf(stderr, "Could not read the ROM from the cart: %s\n", sc64_error());
        return 1;
    }
    uint32_t game_id = 0;
    if (!ra_web_resolve_hash(hash, &game_id)) {
        fprintf(stderr, "Could not look the game up: %s\n", ra_web_error());
        return 1;
    }
    if (game_id == 0) {
        printf("RetroAchievements doesn't know this ROM (hash %s): a different dump of the game may be supported.\n", hash);
        return 0;
    }
    rc_api_fetch_game_data_response_t game;
    if (!ra_web_fetch_game(game_id, &game)) {
        fprintf(stderr, "Could not download the game's achievements: %s\n", ra_web_error());
        return 1;
    }
    printf("RetroAchievements game %lu: %s, %lu achievements (hash %s)\n", (unsigned long) game_id,
           game.title ? game.title : "", (unsigned long) game.num_achievements, hash);
    save_definitions(&game);

    int active = watch_load(&game, view);
    if (!watch_plan()) {
        fprintf(stderr, "Could not give the hook its list: %s\n", sc64_error());
        return 1;
    }
    int ranges = 0, chains = 0, bytes = 0;
    watch_list_size(&ranges, &chains, &bytes);
    printf("Checking %d achievements (memory view: %s); the hook copies %d ranges and follows %d pointer chains, %d bytes a frame.\n",
           active, view == WATCH_VIEW_SWAPPED ? "swapped" : "native", ranges, chains, bytes);
    if (watch_skipped()) {
        printf("Not checked yet: %d achievements that follow pointers in a way the hook can't.\n", watch_skipped());
    }
    printf("Nothing is sent to your account: met achievements are only printed. Ctrl+C stops.\n\n");

    DWORD started = GetTickCount();
    while (!stop) {
        bool running = false;
        if (!hook_poll(watch_frame, NULL, &running)) {
            fprintf(stderr, "\nReading the cart failed: %s\n", sc64_error());
            break;
        }
        if (running) {
            double seconds = (GetTickCount() - started) / 1000.0;
            printf("\r%5.1f frames/s checked, %lu missed, %lu with reads not found  |  %d of %d met   ",
                   hook_frames_seen() / (seconds > 0.001 ? seconds : 0.001), (unsigned long) hook_frames_missed(),
                   (unsigned long) watch_misses(), watch_met(), active);
        } else if (hook_is_old()) {
            printf("\rThe menu on the SD card is older than this program: copy the new sc64menu.n64 to the card.   ");
        } else {
            printf("\rNo hook yet (start the game with ra_test = 1).        ");
        }
        fflush(stdout);
        Sleep(50);
    }
    printf("\nStopped: %lu frames checked, %lu missed, %d achievements met.\n", (unsigned long) hook_frames_seen(),
           (unsigned long) hook_frames_missed(), watch_met());
    rc_api_destroy_fetch_game_data_response(&game);
    sc64_close();
    return 0;
}
