# SummerStation64 handoff

Written 2026-10-07. This is the short version: where the project stands, what
comes next, and what is still open. The full record, with every measurement
and the reasons behind each choice, is `CLAUDE.md` in the top folder. Update
this file whenever a step is finished.

## What this is

SummerStation64 is a themeable game front-end for the SummerCart64, forked
from N64FlashcartMenu and built on libdragon. The owner (Kerillian) tests
every build on a real N64 with a small composite CRT and is learning as the
project goes, so changes are explained in plain language.

## How a step goes

1. Make one change, in a new file where possible, with a small hook in the
   stock file that calls it.
2. Build in the dev container with `make sc64`. Warnings are errors.
3. Stop. The owner deploys from Windows and tests on the console:
   - `.\localdeploy.bat` runs the build from the cart's memory.
   - `.\deploy-sd.bat` puts it on the SD card (N64 switched off).
   - Add `/d` to either to watch the debug log.
   - **Never `.\localdeploy.bat /dur`:** it empties the menu file on the card.
4. When the owner says the test passed, commit on `carousel-ui`.
5. Push to `fork` only when asked. Never push to `origin` (upstream) and
   never merge `n64ever` (a reference fork, read only).

## Where it stands

- Branch `carousel-ui`, working tree clean before this handoff was written.
- `fork/carousel-ui` is at `e9479850`. Two later commits are local only:
  `309a010c` (play counts) and `e124e965` (N64ever notes correction), plus
  the commit that adds this file. **Not pushed; wait to be asked.**
- The branch is 54 commits ahead of upstream `main`: about 4,700 lines added
  across 57 files, most of them in new files.
- The last build, `output/sc64menu.n64`, is 1.9 MB and matches stage 5.

### Finished and tested on hardware

| Area | What works |
|---|---|
| Themes | `theme.ini` loader, text and frame colors, gradient and pattern backgrounds, feature switches a theme can set |
| Games screen | Cover carousel with slide animation, box art, box flip to the back cover, optional previous/next covers, tab bar with clock, title panel with badges, position bar, button hints |
| Game info screen | Blurred cover backdrop, large title, badges, Played / Last played / Players boxes, description |
| Controls | Left/right browse, L/R switch tabs, Z for options, quick launch and hold-A-to-launch |
| Settings | Game-style screen with six categories, every feature as Default / On / Off, saved once on leaving |
| Library | Sort orders, remembered position per folder, tidied game names, play counts |
| Safety | Safe mode (hold Z at boot), friendly crash screen, safe file saving |
| Memory | Half-size background (300 KB saved), small Latin font (about 700 KB saved), measured budget table |
| Tools | Web Theme Maker with share codes (version 7), SD deploy script, `stats:` debug line |

The v0.1 features a player sees are complete.

### The menu redesign

Fast-tracked from two mockups the owner supplied. Stages 1 to 5 and 7 are
done and tested:

1. Games screen layout
2. Badges on the title panel
3. Button hints
4. Game info screen
5. Play counts and last played

Remaining:

6. **Recent and Favorites as cover rows** instead of lists. Large. Proposed
   as two builds, Recent first.
7. **Intro:** done and tested 2026-10-07 (`src/menu/intro.c`). Plays at
   power-on only; details in `CLAUDE.md` under "Menu redesign".

Stage 6 is the only stage left.

## Next steps

1. **Ask the owner whether to start stage 6** (Recent tab first) or the
   asset prep tool (see the open question below).
2. If stage 6: reuse the carousel from `views/browser.c` and the cover cache
   in `carousel_art.c` for the Recent tab in `views/history_favorites.c`.
   That screen still uses its text hints; move it to the button-badge hints
   from `games_ui.c` at the same time.
3. When the owner says the Jumper Pak is ready, give them the "4MB test
   checklist" from `CLAUDE.md` (18 steps), updated with anything added since.
   Every 4MB figure so far is worked out from 8MB runs, not measured.
4. Before publishing v0.1: the owner wants to test Japanese (tall) and
   64DD-shaped cover art.
5. Still in v0.1 and not started: the debug overlay and the PC-side tests for
   the theme parser and share code decoder.

## Open questions

- **Art baked into the ROM.** The owner asked what it would gain. The answer
  given: it removes the PNG decode (the 70-85 ms hitch per cover) and needs
  no setup, but the project would be distributing publishers' artwork, the
  ROM would grow from 1.9 MB to tens of megabytes (slower every boot), and
  only listed games would have art. Recommended instead: the asset prep tool
  already planned for v0.2, which converts the user's own art on a PC to the
  console's format on the SD card. The guess of 15-25 ms per cover is not
  measured. **The owner has not replied.**
- Truncated descriptions: text from the metadata pack arrives cut off.
  Adding "..." was offered; no answer.
- 64DD disk launches are not counted in play stats. Offered; no answer.

## Known problems, not being worked on

- **60 frames a second is out of reach for now.** Each screen needs 17-23 ms
  of work against the 16.7 ms that 60 allows. The cost follows the amount of
  text on screen, which is laid out again every frame. Fixing it means laying
  text out once and reusing it: a project of its own. `frame_rate.c` stays as
  a hidden option at the owner's request.
- **`/dur` empties `sc64menu.n64` on the card.** The stock USB file transfer
  fails; why is unknown. Worked around with `deploy-sd.bat`.
- One frame of about 50 ms roughly every 6 seconds on the Games and Game
  info screens. Cause unknown.
- A theme with `dither = 0` has never been checked with the half-size
  background.
- Storage paths are not profile-aware yet (`folders.ini`, `playstats.txt`).
  Profiles are planned for v0.4.

## Things that are easy to get wrong

- The cart is powered through the USB cable, so it keeps the uploaded menu in
  memory when the console is switched off. "It survived a power cycle" only
  counts with the cable unplugged, or for something stored in a settings
  file.
- The C library's `rename()` always fails on `sd:/`. Use
  `safe_file_replace()`.
- The dev container cannot see USB devices and has no JavaScript runtime, so
  nothing can be deployed from it and the Theme Maker cannot be run in it.
- New `.c` files must be added to the Makefile list or they are not built.
- `tools/` is ignored by git; that is why the Theme Maker is in
  `theme-maker/`.
- The owner dislikes side covers and frame borders on their CRT. Both stay
  as options; side covers are off by default.
- The owner prefers saving memory over a small gain in picture quality, and
  fewer SD card writes over saving at once.
- Do not copy N64ever's box art or game descriptions. Any of its code reused
  keeps its notices and is credited.
