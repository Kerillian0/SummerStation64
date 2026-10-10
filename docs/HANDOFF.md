# SummerStation64 handoff

Written 2026-10-09. This is the short version: where the project stands, what
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

- Branch `carousel-ui`, 74 commits ahead of upstream `main` (about 10,700
  lines added across 114 files, most of them in new files).
- **The fork was last pushed 2026-10-09** (row wrapping round and everything
  before it). `git log fork/carousel-ui..carousel-ui` lists anything newer.
  **Push only when asked.**
- `output/sc64menu.n64` is 27.9 MB because the owner's box art is baked into
  it. Without baked art it is about 2.5 MB.
- The owner's art is in `assets/boxart/source/` and the baked covers in
  `filesystem/art/`. Both are ignored by git. **Never commit them.**

### What the menu does now

| Area | What works (tested on hardware) |
|---|---|
| Tabs | Games, Favorites and Folders by default, Recent optional; the player sets the order in Settings > Tabs; L/R switch |
| Games tab | Games only, from the start folder, as a row of covers with slide animation, box flip, optional side covers, title panel, badges, favorite heart, position bar |
| Folders tab | Plain file browser: everything by real name with sizes; opens pictures, music, text, zips; sets the start folder |
| Recent, Favorites | Cover rows like Games; Z removes a favorite after asking |
| Game info | Blurred cover backdrop, large title, badges, Played / Last played / Players, description |
| Look | Rounded corners, button icons, memory badge (drawn Expansion Pak or Jumper Pak), all switchable |
| Themes | The player's `theme.ini`, or four built-in summer themes (Sunset, Night Drive, Beach, Ocean); Sunset is the stock look |
| Start-up | Intro with a spinning 3D logo (vaporwave or classic), an 80s-style title picture and a tune; fade-in of picture and music; both have settings |
| Covers | From art baked into the menu file (4-7 ms each) or PNGs on the card; a saved list remembers which file is which game |
| Controls | Left/right browse, up/down flip, Z options, START menu on every tab, quick launch and hold-A-to-launch |
| Settings | Game-style screen, seven categories, saved once on leaving |
| Library | Sort orders, remembered position per folder, tidied names, play counts |
| Safety | Safe mode (hold Z at boot), friendly crash screen, safe file saving |
| Sounds | The owner's choice of music, intro tune and settings sound, with credits |

### Scrolling speed (the last piece of work)

| | When first reported | Now |
|---|---:|---:|
| Average frame while stepping through covers | 40-53 ms | 33.4 ms |
| Worst frames | 90-126 ms | 41-44 ms |
| Wait before a cover appears | 0.25 s plus the load | 0.1 s |

What got it there, in order: side covers wait longer, baked art, the saved
game list, reading the saves folder once, less text layout per frame, cover
loading moved to after the frame is drawn, shorter waits, and two fixes to
how frames are shown (2026-10-09): the cap is exactly every second refresh
(it was "30", which cost a 50 ms frame every six seconds on every screen),
and the menu waits for the graphics chip at the end of each frame so a
finished frame is shown at once (`frame_rate.c`). The log's `stats:` line
now gives `work` and `chip` times, and prints `slow frame:` lines.

What is still slow: the fade-in runs at about 15 frames a second (the
see-through black over the whole screen costs the chip about 11 ms); a
cover read from the card costs one 50 ms frame; saving a settings file
costs one frame of 70-80 ms. The Folders tab has the least room (29 ms of
work a frame).

## Next steps

Done since the handoff was written (2026-10-09/10, all tested): row wrapping
round, the frame-timing fixes, covers sliding in on a tab switch, baked art
on Game info, each tab remembering its game, the Game info tidy-up (date,
Expansion Pak badge, series/subtitle, longer description, Details on
START) and the ring tinted from the cover (option `ring_tint`).

1. **Ask the owner what comes next.** Loose ends on offer: a cheaper
   fade-in (it runs at 15 frames a second), the Folders tab's frame cost
   (29 ms of 33), bringing the Theme Maker up to date.
2. **Push the fork when asked.** `git log fork/carousel-ui..carousel-ui`
   lists what is waiting.
3. When the owner says the Jumper Pak is ready, give them the "4MB test
   checklist" from `CLAUDE.md`, updated with anything added since. Every 4MB
   figure so far is worked out from 8MB runs, not measured.
4. **v0.1 is complete (2026-10-10).** Everything on the v0.1 list is
   done and tested on hardware, including the owner's Japanese (tall) and
   64DD cover check, the performance overlay (Settings > System) and the
   PC tests (`sh tests/run.sh`). Since then: the Games tab reads several
   folders (Folders tab, Z, "Add this folder to Games") and covers take
   their own shape. Next is publishing v0.1, when the owner wants to.
5. Done 2026-10-10: the Games filter (C-up / C-down: All / USA / Japan /
   Europe / 64DD). Waiting: a swipe for the Folders tab, put off until it
   fits the memory budget; optionally reading unknown games' headers so
   untagged file names get a region before they are visited.

## Open questions

- **The intro's title picture:** the owner said it looks good and to leave
  it "on the backburner". It was left in the build as it is.
- **More summer ideas** (a banded sun in the intro, a "horizon grid"
  pattern, palm silhouettes, a time-of-day look, summer sounds): the owner
  put these off to v0.4 with the rest of the theme work.
- **The Theme Maker is behind the menu.** It does not know the `ocean`
  background, the built-in themes, or the features added since version 7.
- Older, still unanswered: adding "..." to descriptions that arrive cut off;
  counting 64DD launches in play stats.

## Known problems, not being worked on

- **60 frames a second is out of reach for now.** Each screen needs most of
  a 33 ms frame; the cost follows the amount of text and covers on screen.
- **`/dur` empties `sc64menu.n64` on the card.** The stock USB file transfer
  fails; why is unknown. Worked around with `deploy-sd.bat`.
- A theme with `dither = 0` has never been checked with the half-size
  background.
- Storage paths are not profile-aware yet (`folders.ini`, `playstats.txt`,
  `gameindex.txt`). Profiles are planned for v0.4.
- The Games tab can only change folder through the Folders tab. It is meant
  to show every game on the card once the full metadata index exists.

## Things that are easy to get wrong

- The cart is powered through the USB cable, so it keeps the uploaded menu in
  memory when the console is switched off. "It survived a power cycle" only
  counts with the cable unplugged, or for something stored in a settings
  file. For the same reason the intro shows only on the first start after an
  upload while the cable is in.
- The C library's `rename()` always fails on `sd:/`. Use
  `safe_file_replace()`.
- The dev container cannot see USB devices and has no JavaScript runtime, so
  nothing can be deployed from it and the Theme Maker cannot be run in it.
- New `.c` files must be added to the Makefile list or they are not built.
- After adding or removing files under `filesystem/` by script, delete
  `build/N64FlashcartMenu.dfs`, or the build keeps the old set.
- Text is dropped if its box is lower than one line of its font. Black text
  turns into a blob (the font has a dark outline). 12 px text is the
  smallest that reads on the CRT ("Detected" is fine; a photo made it look
  smeared).
- Measure before fixing speed: put a timing in the log first. Three guesses
  were wrong during the scrolling work.
- A log the owner pastes can be the same one as last time. Compare before
  concluding anything.
- The owner dislikes side covers and frame borders on their CRT (though they
  have had side covers on lately). Both stay as options.
- The owner prefers saving memory over a small gain in picture quality, and
  fewer SD card writes over saving at once.
- Outside pictures and sounds need a source and a licence, recorded in the
  folder's `CREDITS.md`, before they are committed. Do not copy N64ever's
  box art or game descriptions.
- Two things the owner has decided and does not want raised again: the
  intro's logo is the N64 logo's shape, and box art is baked from their own
  pack.
