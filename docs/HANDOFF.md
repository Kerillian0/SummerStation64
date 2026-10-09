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
- **`fork/carousel-ui` is at `701e9e8d`.** Everything since (13 commits,
  from `6b51e292` on, the last being the one that adds this file) is local
  only. **Not pushed; wait to be asked.**
- **One change is built but not tested or committed:** badges for a game the
  menu already knows appear after 0.1 s instead of 0.35 s
  (`src/menu/game_facts.c`).
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
| Average frame while stepping through covers | 40-53 ms | 33-36 ms |
| Worst frames | 90-126 ms | about 64 ms |
| Wait before a cover appears | 0.25 s plus the load | 0.1 s |

What got it there, in order: side covers wait longer, baked art, the saved
game list, reading the saves folder once, less text layout per frame, cover
loading moved to after the frame is drawn, shorter waits. The owner finds it
responsive. What is left: copying a cover in (6-9 ms) still doubles most of
the frames it lands on.

## Next steps

1. **Get the badge timing tested** (the uncommitted change above), then
   commit it.
2. **Ask the owner what comes next.** They paused the work to ask for this
   handoff. On offer when they did:
   - baked art on the Game info screen (it still reads the PNG);
   - remembering that a game has no baked back cover (flipping one looks on
     the card every time, about 17 ms);
   - the row wrapping round, so the last game shows left of the first;
   - covers sliding sideways when switching tabs.
3. **Push the fork when asked.** Thirteen commits are waiting.
4. When the owner says the Jumper Pak is ready, give them the "4MB test
   checklist" from `CLAUDE.md`, updated with anything added since. Every 4MB
   figure so far is worked out from 8MB runs, not measured.
5. Before publishing v0.1: the owner wants to test Japanese (tall) and
   64DD-shaped cover art. Still in v0.1 and not started: the debug overlay
   and the PC-side tests for the theme parser and share code decoder.

## Open questions

- **"Detected" under the memory amount** is in a 12 px font that looked
  smeared in a photo. Asked twice whether to enlarge it, remove it or leave
  it; no answer yet.
- **The intro's title picture:** the owner said it looks good and to leave
  it "on the backburner". It was left in the build as it is.
- **More summer ideas, none chosen yet:** a banded sun in the intro, a
  "horizon grid" background pattern, palm silhouettes, a time-of-day look,
  summer sounds.
- **The remaining slow frames when a cover loads.** Fixing them means
  spreading the copy over two frames. Suggested stopping here unless it
  still bothers the owner.
- **The Theme Maker is behind the menu.** It does not know the `ocean`
  background, the built-in themes, or the features added since version 7.
- Older, still unanswered: adding "..." to descriptions that arrive cut off;
  counting 64DD launches in play stats.

## Known problems, not being worked on

- **60 frames a second is out of reach for now.** Each screen needs most of
  a 33 ms frame; the cost follows the amount of text and covers on screen.
- **`/dur` empties `sc64menu.n64` on the card.** The stock USB file transfer
  fails; why is unknown. Worked around with `deploy-sd.bat`.
- One frame of about 50 ms roughly every 6 seconds on the cover screens.
  Cause unknown.
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
  turns into a blob (the font has a dark outline). 12 px text smears.
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
