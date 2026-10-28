/* PC-side tests for telling hacks from originals (src/menu/rom_hacks.c).
   Run with tests/run.sh. */

#include <stdio.h>
#include <string.h>

#include "rom_hacks.h"

static int failures = 0;
static int checks = 0;

typedef struct {
    const char *name;
    const char *code;   /* "" if the menu has not learned it yet */
    int mb;             /* the file's size */
    bool rom;
    bool expect;
} game_t;

static void run (const char *what, const game_t *games, int count) {
    const char *names[32];
    char codes[32][4];
    bool is_rom[32];
    bool hack[32];
    printf("%s\n", what);
    for (int i = 0; i < count; i++) {
        names[i] = games[i].name;
        memset(codes[i], 0, 4);
        if (games[i].code[0]) memcpy(codes[i], games[i].code, 4);
        is_rom[i] = games[i].rom;
    }
    rom_hacks_find(names, codes, is_rom, count, hack);
    for (int i = 0; i < count; i++) {
        checks++;
        if (hack[i] != games[i].expect) {
            failures++;
            printf("  FAIL: \"%s\" came out as %s\n", games[i].name, hack[i] ? "a hack" : "an original");
        }
    }
}

#define RUN(what, list) run(what, list, (int) (sizeof(list) / sizeof(list[0])))

int main (void) {
    /* The owner's library, from the log of 2026-10-14. */
    static const game_t library[] = {
        { "007 - GoldenEye.v64", "NGEE", 16, true, false },
        { "GoldenEye 007 (U) [!].z64", "NGEE", 12, true, false },
        { "Legend of Zelda, The - Ocarina of Time (USA).z64", "CZLE", 32, true, false },
        { "Legend of Zelda, The - Ocarina of Time (USA)_redux_patched.z64", "CZLE", 32, true, true },
        { "Legend of Zelda, The Missing Link (U) (V1.3).z64", "CZLE", 16, true, true },
        { "Legend of Zelda, The Sealed Palace (U) (V1.3).z64", "CZLE", 32, true, true },
        { "New Tetris, The (U) [!].z64", "NRIE", 12, true, false },
        { "The New Tetris (U) (!).n64", "NRIE", 12, true, false },
        { "Pokemon Snap Station - Kiosk (U) [!].v64", "NPOE", 32, true, false },
        { "Pokemon Stadium (U) [!].z64", "NPOE", 32, true, false },
        { "Star Fox 64 (U) [!].z64", "NFXE", 12, true, false },
        { "Star_Fox_64_(U)_[f1].v64", "NFXE", 12, true, false },
        { "Star Wars - Episode 1 - Racer (U) (!).n64", "NEPE", 32, true, false },
        { "Star Wars Episode I - Racer (U) [!].z64", "NEPE", 32, true, false },
        { "Super Mario 64 (U) [!].v64", "NSME", 8, true, false },
        { "Super Mario 64 (U) [!]_patched.z64", "NSME", 64, true, true },
        { "Super Mario 64 - Return to Yoshi's Island.z64", "NSME", 16, true, true },
        { "Dezaemon 3D [Patched].n64", "NDZJ", 32, true, false },     /* patched, but alone with its code */
        { "Pilotwings 64 (U) [!].z64", "NPWE", 8, true, false },
    };
    RUN("The owner's library", library);

    static const game_t by_name[] = {
        { "Mario Kart 64 (U) [h1C].z64", "NKTE", 12, true, true },
        { "Some Game (Hack).z64", "", 8, true, true },
        { "Smash Remix ROMHACK.z64", "", 64, true, true },
        { "Chameleon Twist (U) [!].z64", "NCTE", 8, true, false },
        { "Hack notes.txt", "", 0, false, false },
        { "South Park - Chef's Love Shack (USA).z64", "NLSE", 8, true, false },  /* a player's report, 2026-10-26 */
        { "Smash_Remix_romhack_v1.z64", "", 64, true, true },
        { "Hacked Game.z64", "", 8, true, true },
    };
    RUN("The file name says it is a hack", by_name);

    static const game_t alone[] = {
        { "Wave Race 64 (U) [!].z64", "NWRE", 8, true, false },
        { "Wave Race 64 (J) [!].z64", "NWRJ", 8, true, false },
        { "Unknown One.z64", "", 8, true, false },
        { "Unknown Two.z64", "", 64, true, false },
        { "Disk Game.ndd", "", 64, false, false },
    };
    RUN("Other regions, unknown codes and disks are left alone", alone);

    printf("rom hacks: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
