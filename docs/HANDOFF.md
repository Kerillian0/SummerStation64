# SummerStation64 handoff

Written 2026-10-14. The short version of where things stand. Conventions,
file layout, every measurement and the reasons behind each choice are in
`CLAUDE.md`; read this first, then that.

## Goal

A themeable game front-end for the SummerCart64 on a real N64, forked from
N64FlashcartMenu (V0.3.4) on libdragon. It must run on a stock 4MB console.
The owner (Kerillian) tests every build on hardware (composite CRT, Expansion
Pak fitted) and is learning as the project goes, so explain changes plainly.

## How a step goes

1. One change, in a new file where possible, with a small hook in the stock
   file. `make sc64` in the dev container (warnings are errors).
2. Stop. The owner deploys from Windows (`.\deploy-sd.bat /d` or
   `.\localdeploy.bat /d`; **never `/dur`**) and reports with the log.
3. "All testing passed" means commit on `carousel-ui`. Push to `fork` only
   when asked; never to `origin`.
4. PC tests: `sh tests/run.sh` (theme.ini reader, hack finder, share codes).

## Current state

- **v0.1 is complete but not published.** The owner publishes only after a
  full test on the 4MB Jumper Pak (not arrived yet). Draft notes:
  `docs/RELEASE_NOTES_v0.1.md`.
- **Fork last pushed 2026-10-14** (`29c09822`).
- ROM hack detection, step 1 (below): tested and committed 2026-10-14.
- The build is 27.9 MB because the owner's box art is baked in. Never commit
  `assets/boxart/source/` or `filesystem/art/`, and never publish a build
  that contains them.

## What changed since the last handoff (2026-10-12)

All tested on hardware and committed unless marked.

| Change | Main files |
|---|---|
| Idle saves take turns (no more 140 ms frame) | `safe_file.c` (`safe_file_idle_turn()`), `folder_memory.c`, `carousel_art.c` |
| Release dates and developers in the game list; Sort By Release Date / Developer, shown under the title | `game_index.c`, `game_learn.c`, `sort_order.c`, `games_ui.c` |
| Folders list slides in on a tab switch | `tabs.c`, `folders_ui.c` |
| Save backups: dated, five per game, never the same twice; "Restore save" on the game's Options | `save_backup.c` (new), `views/load_rom.c` |
| Theme Maker version 8: today's Games/Folders preview, Ocean, built-in presets, four look switches (published) | `theme-maker/theme-maker.html`, `tests/share_code_test.js` |
| Random game on the Games tab's Options, and Options menus on Recent and Favorites | `views/browser.c`, `views/history_favorites.c`, `cover_row.c` |
| ROM hacks get a "Hack" badge and a "Hacks" filter step | `rom_hacks.c` (new), `games_filter.c`, `games_ui.c`, `views/browser.c`, `tests/rom_hacks_test.c` (new), `tests/run.sh`, `Makefile` |

### ROM hacks, step 1 (done and tested)

The owner chose: badge and filter first, then one cover per game with a
version picker (step 2, with a setting between the two looks); detection by
automatic rules **and** a folder the player marks as holding hacks.

How a hack is told now (`rom_hacks.c`):
1. The name says so: "hack", "romhack", "redux", or a `[h1]` tag.
2. Files sharing a game code: their names are compared word by word
   (brackets and little words left out, any order). The name whose words
   turn up in most of the others is the original; same words = another copy
   of it; the rest are hacks. No name holding another = nothing is a hack.

Expected on the owner's card: 5 hacks (both Super Mario 64 hacks, Ocarina
Redux, The Missing Link, The Sealed Palace). The PC test holds all 19 of the
owner's same-code file names and passes. The log prints each shared code's
files and what each was taken for (`games filter: code ...`).

## What failed or did not work

- **Hack rule 2, first try:** "the less tidy name of two same-code files is
  the hack" flagged second copies of Star Fox 64, GoldenEye, Racer, The New
  Tetris; "patched" in a name flagged the Dezaemon 3D translation.
- **Hack rule 2, second try:** "the larger file is the hack" flagged a 16 MB
  GoldenEye copy and took Ocarina itself for a hack (The Missing Link is
  smaller). Lesson: the owner keeps duplicate dumps, and file size says
  nothing about which is the original.
- **Save backups, first design:** three numbered copies; copy 1 normally
  equalled the save in use, so restoring it did nothing and duplicates
  filled the slots. Replaced by dated, deduplicated copies.
- **Frame Borders does nothing** on the redesigned tabs, on the console or
  in the Theme Maker. Open; the owner wants it fixed later, look undecided.
- **Not explained yet:** the idle game reader's frames climb from 33 to 39
  ms; odd slow saves of 120-130 ms. Both are on the pre-v1.0 optimization
  list in `CLAUDE.md`, as is the Folders tab (work 27 / chip 29 ms).

## What the next session should do

**The owner's plan from 2026-10-14: work version by version, one item at a
time, in the order of the release plan in `CLAUDE.md`** (each item marked
done, partly done or not started). v0.1 waits on the Jumper Pak, so the
work is in v0.2. The fork was pushed 2026-10-14 (`29c09822`).

1. **The 4MB checklist** (the Jumper Pak arrived 2026-10-20; the critical
   steps were given to the owner, run on the Full font, the tightest case).
   Then v0.2: perspective covers, then the live background. (Pak badges and
   the memory note were done 2026-10-20.)
2. Hacks (v0.3, picked up when v0.3 is reached): the marked folder
   (Folders tab, Z, "This folder holds hacks").
3. **Hacks, step 2:** one cover per game with a version picker, and a setting
   to choose that or separate covers. Stand-alone hacks with their own art
   (the owner's examples: The Sealed Palace, The Missing Link) must be able
   to stay independent. The owner will compile a list of hacks that are
   practically their own games; offer to research a starting list. The
   companion app and setup wizard should later set this up for the player.
4. Push when asked. When the Jumper Pak arrives, give the owner the 4MB
   checklist in `CLAUDE.md` (26 steps); publish v0.1 only after it passes
   and the owner says so (clean build without baked art, as described in
   `CLAUDE.md`).

Still waiting on the owner's decisions: what a frame should look like on the
redesigned screens (Frame Borders), and where genre would come from (the
metadata pack has none).
