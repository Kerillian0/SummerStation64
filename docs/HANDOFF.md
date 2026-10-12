# SummerStation64 handoff

Written 2026-10-12. This is the short version: where the project stands, what
comes next, and what is still open. The full record, with every measurement
and the reasons behind each choice, is `CLAUDE.md` in the top folder. Update
this file whenever a step is finished.

## What this is

SummerStation64 is a themeable game front-end for the SummerCart64, forked
from N64FlashcartMenu (based on its V0.3.4) and built on libdragon. The owner
(Kerillian) tests every build on a real N64 with a small composite CRT and is
learning as the project goes, so changes are explained in plain language.

## How a step goes

1. Make one change, in a new file where possible, with a small hook in the
   stock file that calls it.
2. Build in the dev container with `make sc64`. Warnings are errors.
3. Stop. The owner deploys from Windows and tests on the console:
   - `.\localdeploy.bat` runs the build from the cart's memory.
   - `.\deploy-sd.bat` puts it on the SD card (N64 switched off).
   - Add `/d` to either to watch the debug log.
   - **Never `.\localdeploy.bat /dur`:** it empties the menu file on the card.
4. When the owner says "All testing passed", commit on `carousel-ui`.
5. Push to `fork` only when asked. Never push to `origin` (upstream) and
   never merge `n64ever` (a reference fork, read only).

PC-side tests, no console needed: `sh tests/run.sh` (the theme.ini reader and
the Theme Maker's share codes). Run them after touching either.

## Where it stands

- Branch `carousel-ui`, about 97 commits ahead of upstream `main` (about
  13,600 lines added across 130 files, most of them in new files).
- **The fork was last pushed 2026-10-12**, with everything up to the ring
  waiting for a cover's color. Nothing is uncommitted. `git log
  fork/carousel-ui..carousel-ui` lists anything added since. Push only
  when asked.
- **v0.1 is complete but not published.** The owner will publish only after
  a full test on the 4MB Jumper Pak, the most common setup; it had not
  arrived on 2026-10-12. The draft notes for players are
  `docs/RELEASE_NOTES_v0.1.md`.
- `output/sc64menu.n64` is 27.9 MB because the owner's box art is baked into
  it. Without baked art it is about 2.5 MB. The owner's art is in
  `assets/boxart/source/` and the baked covers in `filesystem/art/`; both
  are ignored by git. **Never commit them, and never publish a build that
  contains them.**

### What the menu does now

| Area | What works (tested on hardware unless noted) |
|---|---|
| Tabs | Games, Favorites and Folders by default, Recent optional; order set in Settings > Tabs; L/R switch, covers slide in; each tab returns to its game |
| Games tab | Games only, from the start folder plus folders the player adds; cover row with slide, box flip, optional side covers and wrap-round; C-up/C-down filter by region or 64DD |
| Folders tab | Plain file browser; opens pictures, music, text, zips; adds folders to Games; sets the start folder |
| Recent, Favorites | Cover rows like Games; Z removes a favorite after asking |
| Covers | Baked art (4-7 ms) or PNGs on the card; drawn in their own shape; ring follows the shape and, optionally, the cover's color |
| Game info | Blurred cover backdrop, series and subtitle, badges, Played / Last played / Players, description ending in "..." when cut, one Details pop-up on START |
| Game knowledge | `gameindex.txt` remembers each game's code and badge facts; unknown games are read while the Games tab is idle |
| Look | Rounded corners, button icons, memory badge, favorite heart, all switchable; four built-in summer themes or the player's `theme.ini` |
| Start-up | Intro with a spinning 3D logo, title picture and tune; fade-in at full speed; both have settings |
| Controls | Left/right browse, up/down flip, Z options, START menu on every tab, quick launch and hold-A-to-launch |
| Settings | Game-style screen, seven categories, saved once on leaving |
| Safety and tools | Safe mode (hold Z at boot), friendly crash screen, safe file saving, Performance Overlay, PC tests |

### Speed, as last measured (8MB, 240p, small font)

| | When first reported | Now |
|---|---:|---:|
| Average frame while stepping through covers | 40-53 ms | 33.4 ms |
| Worst frames while stepping | 90-126 ms | 41-44 ms |
| Start-up fade | 15 frames a second | full speed, one slow frame |
| Wait before a cover appears | 0.25 s plus the load | 0.1 s |

Per frame at rest: Settings work 14 / chip 16 ms; Games 22 / 24 ms; Game
info 16 / 18 ms. A frame has 33.4 ms, and the chip needs to be done by about
30. The Folders tab was 29 ms of work before its text was laid out once; the
saving has **not been measured** (the owner's log was cut short).

What is still slow, all one frame each: a cover read from the card (about
50 ms, then three while its PNG unpacks); saving a settings file (70-80
ms); saving the game list and the folder position in the same idle frame
(about 140 ms).

## Next steps

1. **Ask the owner what comes next.** They said "keep developing". On offer:
   - game details in the index (year, genre, publisher), the next piece of
     v0.2, which opens sorting and filtering by them and "recently added";
   - a random game button (v0.3);
   - save backups (v0.2);
   - staggering the two idle saves (the 140 ms frame);
   - a swipe for the Folders tab, put off by the owner until it fits the
     memory budget (sliding only what is below the tab bar needs none);
   - bringing the Theme Maker up to date.
2. **Push the fork when asked.**
4. **When the Jumper Pak arrives:** give the owner the "4MB test checklist"
   from `CLAUDE.md` (24 steps). Every 4MB figure so far is worked out from
   8MB runs.
5. **Publishing v0.1, after that test and only when the owner says:** make
   a clean build without baked art (move `filesystem/art/` aside, delete
   `build/N64FlashcartMenu.dfs`, `make sc64 MENU_VERSION="SummerStation64
   v0.1"`), then rebuild the owner's own copy with the art. The owner
   creates the GitHub release with `docs/RELEASE_NOTES_v0.1.md`.

## Open questions

- **In the draft release notes:** whether the "made with the help of Claude"
  line goes in now (the roadmap has an AI disclosure under v0.5), and the
  Theme Maker link (the online one is a private page; the draft points to
  the HTML file in the repository).
- **Tint Ring From Art** defaults to Game Info; the owner uses Everywhere
  and may want that as the stock setting.
- **Wrap File List** is off by default, so the cover row's wrap-round is not
  seen unless switched on.
- **The Theme Maker is behind the menu.** It does not know the `ocean`
  background, the built-in themes, or the options added since version 7.
- Older, still unanswered: counting 64DD launches in play stats.

## Known problems, not being worked on

- **60 frames a second is out of reach for now.** Each screen needs most of
  a 33 ms frame.
- **`/dur` empties `sc64menu.n64` on the card.** The stock USB file transfer
  fails; why is unknown. Worked around with `deploy-sd.bat`.
- **PAL has never been tried.** The frame cap comes out as 25 a second there.
- A theme with `dither = 0` has never been checked with the half-size
  background.
- Storage paths are not profile-aware yet (`folders.ini`, `playstats.txt`,
  `gameindex.txt`, `gamefolders.txt`). Profiles are planned for v0.4.
- Other flashcarts (64drive, ED64) are not supported by this fork.

## Things that are easy to get wrong

- The cart is powered through the USB cable, so it keeps the uploaded menu in
  memory when the console is switched off. "It survived a power cycle" only
  counts with the cable unplugged, or for something stored in a settings
  file. For the same reason the intro shows only on the first start after an
  upload while the cable is in.
- The C library's `rename()` always fails on `sd:/`. Use
  `safe_file_replace()`.
- The dev container cannot see USB devices, so nothing can be deployed from
  it. It does have gcc, Python with Pillow, and Node.js.
- New `.c` files must be added to the Makefile list or they are not built.
- After adding or removing files under `filesystem/` by script, delete
  `build/N64FlashcartMenu.dfs`, or the build keeps the old set.
- Text is dropped if its box is lower than one line of its font. Black text
  turns into a blob (the font has a dark outline).
- Every frame must end with `frame_rate_end_frame()`, or finished frames are
  shown late. Ask for half the refresh rate, never "30".
- Measure before fixing speed, and know which is slow: `work` (processor) or
  `chip` (graphics). See-through fills over large areas are what cost the
  chip; multiply colors as things are drawn where possible.
- On the Games tab an entry's name carries its folder. Use
  `entry_file_name()` for anything shown or sorted, and never assume one
  "current folder" there.
- A log the owner pastes can be the same one as last time, and is cut at
  50,000 characters. Compare before concluding anything.
- The owner prefers saving memory over a small gain in picture quality, and
  fewer SD card writes over saving at once.
- Outside pictures and sounds need a source and a licence, recorded in the
  folder's `CREDITS.md`, before they are committed. Do not copy N64ever's
  box art or game descriptions.
- Two things the owner has decided and does not want raised again: the
  intro's logo is the N64 logo's shape, and box art is baked from their own
  pack.
