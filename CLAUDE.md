# SummerStation64 — project notes for Claude Code

SummerStation64 is an unofficial, themeable game front-end for the SummerCart64,
built as a fork of N64FlashcartMenu (by Polprzewodnikowy and contributors) on
libdragon. Think ES-DE / Pegasus, on a real N64.

## How we work
- One change at a time. After each change: build, fix errors, then stop so the
  user can test on real hardware (small CRT, composite) before the next step.
- Explain each change briefly in plain language; the user is learning as we go.
- Keep the fork easy to merge with upstream: put features in new files and keep
  edits to original files small (one-line hooks where possible).
- Branch: `carousel-ui`. Commit after each working step.
- Remotes: `origin` is upstream (Polprzewodnikowy/N64FlashcartMenu) — fetch
  only, never push there. `fork` is the user's public fork
  (https://github.com/Kerillian0/SummerStation64); `carousel-ui` tracks
  `fork/carousel-ui`. Push only when the user asks.
- `n64ever` is a third remote, for reading only (its push address is
  disabled): https://github.com/bjerreman/N64FlashcartMenu-N64ever , another
  fork of the same menu, based on upstream V0.3.2. Never merge it. What it
  has and how it maps to our roadmap is in `docs/n64ever-notes.md`. Do not
  copy its baked-in box art or game descriptions; keep its notices on any
  code reused and credit N64ever in the acknowledgements.

## Build and test
- Build inside this dev container: `make sc64`
- Output: `output/sc64menu.n64` — the user copies it to the SD card root.
  (The default `make` target only builds the generic ROM.)
- Compiler treats warnings as errors. On this toolchain `int32_t` is `long`:
  cast to `(int)` for `%d` in printf-style calls.
- New .c files must be added to the source list in the Makefile next to
  `menu/sound.c \` (same format, trailing backslash).

### Sending a build to the cart over USB (works, set up 2026-10-05)
The dev container can't see USB devices and can't run the Windows tool, so
Claude builds in the container and the user deploys from Windows.
- `sc64deployer.exe` lives in `tools\sc64` (not committed). Cart firmware must
  be v2.20.2 or newer (`.\sc64deployer.exe info` shows it).
- Run these in a Windows PowerShell opened outside VS Code, from the repo's
  top folder, with the SC64's USB cable connected:
  - `.\localdeploy.bat` — sends the build into the cart; toggle the N64's
    power to boot it. The SD card is not changed.
  - **Do not use `/dur` for now: it leaves `sc64menu.n64` on the card empty
    (see Status).** What it is meant to do:
    `.\localdeploy.bat /dur` — same, then asks the running menu to save
    `output\sc64menu.n64` to the SD card as `/sc64menu.n64` and restart, and
    stays connected showing the menu's `debugf` output. **The menu must
    already be running when this is started, and the power must not be
    toggled** (ignore the script's "toggle power" message in this mode): the
    copy and restart are commands the menu itself carries out
    (`usb_comm.c`). If the log shows `Debug data write dropped due to
    timeout`, the menu wasn't listening, nothing was written to the SD card,
    and the next power cycle boots the old build from the card. Confirmed
    on hardware 2026-10-06: with the console on and the menu showing, the
    build is saved (one frame of about 1.6 s while it writes) and the menu
    restarts. That "confirmation" was wrong: the menu restarted from the
    cart's memory, and the single `dropped due to timeout` line was the file
    transfer failing.
  - `.\deploy-sd.bat` (ours, added and tested 2026-10-06) — **the way to put
    a build on the card.** Copies it with the deployer's own
    `sd upload <SRC> [DST]` command, with the N64 switched off, then shows
    the file's size with `sd stat` and runs `reset`. `/d` also opens the debug
    log afterwards. Tested with the USB cable unplugged afterwards: the menu
    returns after launching a game and resetting.
- In PowerShell a program in the current folder needs `.\` in front.

## Hard constraints
- Target a stock N64 **without** the Expansion Pak (4MB). Everything must run on
  real hardware. 8MB-only extras are allowed but must be gated by
  `features_available()` and degrade gracefully.
- Every optional feature is a user toggle. Layers, most specific wins:
  built-in default → theme `[features]` → user setting.
- Animations must be time-based (PAL runs at 50Hz).
- Write files safely (temp file, then rename). Users power off abruptly.
- Never rename the boot file `sc64menu.n64`. Keep SD folders generic
  (`sd:/menu/...`), not named after the project.
- Display name lives in one constant (planned: `MENU_DISPLAY_NAME`).

## Where to start
`docs/HANDOFF.md` is the short version of this file: where the project
stands, what is next, and what is still open. Read it first in a new session,
then come back here for the detail. Update it whenever a step is finished.

## Key context
How the menu is put together now (2026-10-09):
- **Tabs.** Up to four, in an order the player chooses (Settings > Tabs;
  default Games, Favorites, Folders; Recent can be added). L/R step
  through them. The menu opens on the first.
- **Games** shows only games, as a row of covers, from one folder: the
  start folder. **Folders** is the plain file browser, where everything
  else is opened and the start folder is chosen. The two are the same
  screen and mode (`MENU_MODE_BROWSER`) showing two lists, so every stock
  "back to the browser" lands on whichever was showing.
- **Recent** and **Favorites** are cover rows too. All four share the tab
  bar, title panel, badges and button hints from `games_ui.c`.
- **Where a cover comes from:** art baked into the menu file if the user
  baked theirs (4-7 ms), else the PNG on the SD card (80-120 ms). The
  game's code and badge facts come from a saved list (`gameindex.txt`)
  after the first visit.
- **Start-up:** an intro (spinning logo, title picture, tune) at power-on,
  then the first tab fades in. Both have settings.
- **Themes:** the player's `theme.ini`, or one of four built-in summer
  themes; Sunset is the stock look.
- **Three kinds of setting:** features (on/off, a theme may set them,
  `features.ini`), options (choices, the player's alone, `options.ini`),
  and the stock menu's own settings (`config.ini`).

The owner and how they work:
- Tests every build on a real N64 (Expansion Pak fitted, 240p output, small
  Latin font) with a small composite CRT, and reports with photos, short
  recordings and the debug log. Asks for one thing at a time and often
  adds ideas while testing; build what was asked, then offer the rest.
- Is learning as the project goes: explain in plain words what changed,
  why, and what to check.
- Prefers saving memory over a small gain in picture quality, and fewer SD
  card writes over saving at once. Dislikes clutter on screen.
- Decides art and trademark questions themselves once told the facts. Two
  standing decisions: the intro uses the N64 logo's shape (do not raise it
  again), and box art is baked from the owner's own pack, never committed.
- A 4MB Jumper Pak is on the way; until then every 4MB figure is worked
  out, not measured.

## Where things are
```
CLAUDE.md                  this file: the full project record
Makefile                   source list (add new .c files beside menu/sound.c),
                           the extra font rules (Latin, Title, Title20,
                           Small) and the IMAGES list with each picture's
                           format
localdeploy.bat            stock: run a build from the cart's memory (no /dur)
deploy-sd.bat              ours: put a build on the SD card
assets/fonts/              Firple-Bold.ttf and the charset-*.txt lists the
                           fonts are built from
assets/sounds/             the menu's sounds, with CREDITS.md and
                           make_intro.py (the first, generated intro tune)
assets/images/             icons and patterns (.png, built into sprites),
                           make_icons.py and make_wordmark.py, which draw
                           them, CREDITS.md, and wordmark/ (two fonts with
                           their licences, used only by make_wordmark.py)
assets/boxart/             README.md and make_baked_art.py: bake the user's
                           own box art into the menu file
assets/boxart/source/      the user's art (their SD card's menu/metadata
                           folder); ignored by git, never commit it
filesystem/                what gets packed into the ROM; generated files
                           here are ignored by git
filesystem/art/            baked covers made by make_baked_art.py; ignored
docs/HANDOFF.md            current state and next steps
docs/n64ever-notes.md      what the N64ever fork has, mapped to our roadmap
docs/*.md (numbered)       upstream's user documentation, untouched
theme-maker/               web Theme Maker source (one HTML file) + README
tests/                     PC-side tests: sh tests/run.sh (gcc + Node.js)
tools/sc64/                sc64deployer.exe, not committed (tools/ is ignored)
output/sc64menu.n64        the build
libdragon/                 the SDK, a submodule; do not edit
src/menu/                  menu code; our new files sit beside the stock ones
src/menu/views/            one file per screen
src/menu/ui_components/    stock shared drawing code (small hooks only)
```
Our files in `src/menu/`, by what they are for:
| Area | Files |
|---|---|
| Themes and look | `theme`, `theme_parse` (reads theme.ini; tested on the PC), `builtin_themes`, `title_font`, `font_choice` |
| Settings and switches | `menu_features` (on/off, themes may set), `menu_options` (choices, player only), `views/settings_menu` |
| Tabs and the cover screens | `tabs`, `games_ui` (tab bar, title panel, hints, rounded boxes, icons), `carousel.h` + the carousel in `views/browser.c`, `cover_list`, `cover_row`, `folders_ui`, `start_menu`, `controls` |
| Covers and game data | `games_folders` (which folders the Games tab reads), `carousel_art`, `baked_art`, `game_index`, `game_facts`, `display_name`, `sort_order`, `folder_memory`, `play_stats` |
| Game info screen | `game_info_ui`, `art_tint` (ring color from the cover, also used by the cover rows) |
| Start-up | `intro`, `intro_logo`, `menu_name.h`, `safe_mode` |
| Safety and tools | `safe_file`, `crash_screen`, `debug_stats` (log lines and the performance overlay), `frame_rate` |

All of these are ours and free to change. `views/features_menu` is ours too
but no longer reachable.

Stock files we have edited (keep these edits small): `menu.c`, `actions.c`,
`fonts.c/.h`, `rom_info.c/.h`, `ui_components/background.c`, `common.c`,
`constants.h`, `views/browser.c` (the carousel lives here, the one large
edit), `views/history_favorites.c`, `views/load_rom.c`, `views/load_disk.c`,
`views/startup.c`, `views/credits.c`, `views/settings_editor.c`. `git diff --stat origin/main..carousel-ui` lists
them all.

Files the menu keeps on the SD card, all under `sd:/menu/`:
| File | Holds | Written by |
|---|---|---|
| `theme/theme.ini` (or `theme.txt`) | the theme; read only | the user / Theme Maker |
| `features.ini` | the player's On/Off choices | `menu_features.c` |
| `options.ini` | `sort_order`, `font`, `frame_rate_experiment`, `intro`, `fade`, `intro_logo`, `theme`, `tab1`..`tab4`, `ring_tint`, `perf_overlay` | `menu_options.c` |
| `folders.ini` | selected entry per folder | `folder_memory.c` |
| `playstats.txt` | play count and last played | `play_stats.c` |
| `gameindex.txt` | which file is which game, and its badge facts; safe to delete | `game_index.c` |
| `gamefolders.txt` | folders added to the Games tab, one per line | `games_folders.c` |
| `metadata/` | box art and `metadata.ini` per game; read only | the user's metadata pack |

## Conventions for new code
- A feature is one new `name.c` + `name.h` pair in `src/menu/`, with a
  comment at the top of the header saying what it does in plain words, and
  the smallest possible hook in the stock file that calls it.
- A new on/off feature: add it to the table in `menu_features.c`, add a
  `FEATURE` row to a category in `views/settings_menu.c`, and decide whether
  the Theme Maker should offer it (personal habits are left out). Other kinds
  of setting go in `menu_options.c` with a `CHOICES` row.
- Read a feature with `features_enabled()`. Anything that only works with
  the Expansion Pak also checks `features_available()`.
- Save with a temp file and `safe_file_replace()`, never the C `rename()`.
  Save rarely: on leaving a screen or after a few idle seconds, not on every
  press. Loaders fall back to defaults when a file is missing.
- Time everything with the clock (`get_ticks_ms()`), never by counting
  frames.
- Colors come from the theme (`theme.h`), not fixed values. Keep text at the
  body size or larger and avoid 1 px details: the target is a composite CRT.
  Stay inside the safe area; the Games screens use
  `GAMES_UI_CONTENT_X0/X1`.
- Memory: free what a screen loaded when leaving it, cope with a failed
  allocation by falling back (placeholder, smaller picture), and check the
  cost against the memory budget below.
- Loading work (art, game lookups) waits until the selection has rested
  (250-350 ms) and does one thing per frame.
- Log with `debugf`; the `stats:` line is how memory and frame time are
  read on hardware.
- Commit messages: one plain sentence saying what changed for the player.

Lessons that cost a hardware round each (do not repeat them):
- **Text boxes:** `rdpq` drops text whose box is lower than one line of its
  font. Give every text box at least the font's line height.
- **Never draw black text with the menu font.** It has a dark outline, so
  black letters run into a blob. Use `STL_WHITE` on colored fills.
- **Small text:** 12 px (`FNT_SMALL`) is at the limit for composite; use it
  sparingly. ("Detected" looked smeared in a photo, but the user says that
  was the photo: it reads fine on the CRT and stays as it is.)
- **Measure before fixing speed.** Three guesses about what made scrolling
  slow were wrong or half right. Put a `debugf` timing round the suspect and
  read the log first.
- **The frame budget is tight.** The Games screen with five covers uses
  most of its 33 ms; a few extra milliseconds doubles the frame to 66 ms.
  Do slow work (loading art) after `rdpq_detach_show()`, so the graphics
  chip draws meanwhile, and keep per-frame text layout down (remember
  widths, keep laid-out paragraphs).
- **Reading the SD card is slow** (15-20 ms for one file check, 80+ ms to
  find and open a picture). Read a folder once and remember, or keep a
  saved list (`game_index`), instead of asking per game.
- **The data cache is 8 KB.** A lookup table bigger than that makes a
  per-pixel loop slow and its speed erratic.
- **`sprite_load` halts the menu** if the file is missing or memory runs
  out. Check the file exists and memory is free first (`baked_art.c`).
- **The build only repacks the ROM's files when a listed one changes.**
  After adding or removing files under `filesystem/` by script, delete
  `build/N64FlashcartMenu.dfs`.
- **Check the maths on the PC first** for anything drawn by sums (the logo,
  the ocean): a Python copy rendered with Pillow catches most mistakes
  before a hardware round. Pillow is installed in the dev container
  (`sudo apt-get install python3-pil` if it is ever missing).
- **A log or a GIF the user pastes can be a repeat.** Compare it with the
  last one before drawing conclusions. GIFs can be split into frames with
  Pillow to look at.
- **Pictures and sounds from outside:** find the source and licence before
  committing, and record them in the folder's `CREDITS.md`. Photos and
  other people's artwork are redrawn from scratch instead.

## What we've changed so far
- `src/menu/views/browser.c` — carousel prototype (`BROWSER_CAROUSEL` switch,
  `carousel_draw()`), uses theme colors. Slide animation (`carousel_slide()`):
  160 ms, clock-based, ease-out; covers are drawn at fractional positions and
  the selection ring stays fixed in the center. Moves less than 120 ms apart
  (held direction, fast scroll) snap instead of sliding. Feature
  `carousel_animation` (default on), "Cover Slide" in Settings > Display. Uses
  no extra memory: sliding covers are placeholders, art loads after settling.
- `src/menu/carousel_art.c/.h` — box art on the covers, as a five-slot cache
  (selected cover plus two either side when `side_covers` is on; one slot
  otherwise). Reads the game code from the ROM header itself and reuses the
  stock boxart loader. One decode at a time (the PNG decoder handles one
  image), started 250 ms after the selection settles, most important cover
  first. Moving one step reuses the cached pictures. Side covers only load
  art while 384 KB of heap stays free (`SIDE_ART_MIN_FREE`), so they fall
  back to placeholders when memory is short. Everything is freed when leaving
  the browser.
  Feature `cover_art` (default on). Up/down (when `updown_scroll` is off)
  turn the box over to `boxart_back.png`: the cover squashes flat (110 ms),
  the front is freed, the back is decoded, and the cover opens again, so a
  flip adds no memory. Falls back to the front if a game has no back picture.
- `src/menu/safe_mode.c/.h` — hold Z at boot (read once, synchronously, right
  after `joypad_init()` in `menu.c`). Skips theme.ini/theme.txt, the theme and
  user feature layers, and the custom background image. Shows an orange
  "Safe Mode" label on the Files screen only, centered between the tabs and
  the cover (in `carousel_draw()`). Nothing on
  the SD card is changed.
- `src/menu/crash_screen.c/.h` — friendly crash screen (exception handler
  registered at the top of `menu_init()`), drawn with libdragon's CPU
  `graphics_*` calls on a 320x240 surface via `vi_show()`. Needs
  `vi_write_end_forced()` + `vi_reset()` first (as libdragon's inspector
  does), or the picture comes out repeated and striped. START continues to
  libdragon's technical inspector. Failed assertions still go straight to the
  inspector (libdragon owns that syscall range).
- `src/menu/views/settings_menu.c/.h` — game-style settings screen
  (categories left, that category's settings in the middle, help text below).
  Replaces the stock editor via one line in the `menu.c` view table; the stock
  `settings_editor.c` and the older `features_menu.c` submenu stay in the
  source but are no longer reachable. Features show `Default (On/Off)` when
  the player hasn't chosen, so it's visible whether the theme or the player
  decides. Rows are table-driven: add a `SWITCH`/`FEATURE`/`ACTION`/`INFO`
  line to a category. Eight rows fit above the description; longer categories
  scroll, with a thin scroll bar at the right edge (track in the theme's
  `tab_inactive`, thumb in `accent`); Display has twelve rows and scrolls. The screen reopens where it was left
  (feature `remember_settings`, default on, this power-on only). Categories:
  Display, Controls, Sound, Tabs, Library, Files, System. Changes are held in memory and written once, when
  leaving the screen or after 5 s without a change (the user asked for fewer
  SD card writes); `features_user_change()`/`_flush()` and
  `options_change()`/`_flush()` exist for that.
- `src/menu/debug_stats.c/.h` — prints a `stats:` line to the debug log every
  two seconds (heap size, used, free, average and worst frame time), restarted
  on each screen change. One-line hook in the `menu.c` main loop. Screen
  numbers are `menu_mode_t` values: 2 Files, 9 Settings, 15 game info,
  21 Favorites, 22 History; the Games and Folders tabs are both 2. Read
  it with `localdeploy.bat /d` or `deploy-sd.bat /d` (never `/dur`).
- `src/menu/folder_memory.c/.h` — remembers the selected entry per folder
  (16 most recent folders, by entry name). Restores it when a folder is
  entered with the selection still on the first entry, so the stock "select
  the folder you came out of" behaviour is left alone. Saved to
  `sd:/menu/folders.ini` (temp file + rename) when leaving the Files screen,
  and after the selection has rested for 3 s, and only if something changed. Feature `remember_selection`
  (default on), "Remember Position" in Settings > Files. Two hook lines in
  `view_browser_display()`. Uses 8 KB of static memory. The file path is not
  profile-aware yet (see "Decide early").
- `src/menu/sort_order.c/.h` — file list order: Type (stock), Name A-Z,
  Name Z-A, Recently Played (games from the 8-entry history that live in the
  shown folder come first, then Name A-Z). One line in `load_directory()`
  swaps the stock `qsort` for `sort_order_apply()`; archive listings keep the
  stock order. No dates are available without a slow per-file lookup, so
  "newest first" waits for the metadata index.
- `src/menu/menu_options.c/.h` — the player's non-switch preferences
  (`sort_order`, `font`, `intro`, `fade`, `intro_logo`, `theme`,
  `tab1`..`tab4`, and the hidden `frame_rate_experiment`), saved to `sd:/menu/options.ini` (temp file + rename).
  Themes cannot set these. The settings screen shows them with the `CHOICES`
  row type (A cycles through the values).
- `src/menu/safe_file.c/.h` — `safe_file_replace(temp, final)` swaps a freshly
  written temp file into place with FatFs `f_unlink` + `f_rename`. **The C
  library's `rename()` does not work on `sd:/` in this libdragon** (it always
  fails), which went unnoticed from step 3b until 2026-10-06 because the
  loaders fall back to the temp file when the final one is missing. All three
  settings files (features, options, folders) use this helper now.
- `src/menu/display_name.c/.h` — `display_name(entry)` gives the name to show
  for a game: without its file extension (feature `hide_extensions`) and with
  a trailing article moved to the front (feature `tidy_titles`: "Legend of
  Zelda, The - Ocarina of Time" -> "The Legend of Zelda - Ocarina of Time";
  also ", A" and ", An"). Both default on. Feature `hide_tags` (default off,
  because it makes regional versions look alike) drops every `(...)` and
  `[...]` group. Games only; files are not renamed.
  Used by the carousel, the History and Favorites lists
  (`display_name_file()`), and the game and disk info screens
  (`display_name_info()`). The game info Options menu has "Show/hide real
  file name", which switches both info screens to the real name until it is
  chosen again or the console is switched off. Sorting still goes by the file
  name.
- `theme-maker/theme-maker.html` — source of the web Theme Maker (one
  file, no build step), added to the repo 2026-10-06. Published as the
  claude.ai artifact https://claude.ai/artifact/HxMPCpbZTSRZURJfWswAma : edit
  this file, then publish it to that same address. See the README beside it.
  Version 7 (2026-10-06) offers `hide_extensions`, `tidy_titles` and
  `hide_tags`, shows them in the preview on sample game names, and writes v3
  share codes when one of them is set. It leaves out `remember_selection` and
  `remember_settings` on purpose: those are personal habits, not theme
  choices. Version 7 is unchecked in a browser; only the v3 byte layout was
  checked, with a Python port of the encode/decode arithmetic.
- `src/menu/font_choice.c/.h` — picks the built-in font at startup: the full
  one (`Firple-Bold.font64`, 760 KB in RAM, includes Japanese) or the small
  Latin one (`Firple-Bold-Latin.font64`, 64 KB, built from
  `assets/fonts/charset-latin.txt` by an added Makefile rule). Option `font`
  in `options.ini`: Auto (small without the Expansion Pak, full with it),
  Full, Latin Only (shown to the player as "Character Set" in Settings >
  System, where the user wanted it because it exists to save memory); applies
  after a restart. One
  line changed in `fonts.c`. A custom font on the SD card still wins.
- `src/menu/games_ui.c/.h` — shared pieces of the redesigned Games screens
  (see "Menu redesign"): tab bar with the clock and memory badge, title
  panel with badges and the favorite heart, position bar, button hints
  with icons, and the rounded boxes all of these are drawn with. Colors come from existing theme keys (`tab_active`,
  `tab_inactive`, `accent`, `panel`). Used by the carousel in `browser.c` and
  by `history_favorites.c` in place of the stock tabs and frame.
- `src/menu/frame_rate.c/.h` — option `frame_rate_experiment` in
  `options.ini`: 0 for 30 (the stock cap, default) or 1 for 60. **Not shown in Settings**, because no screen can
  hold 60 (see Status); it stays as a hidden switch for experiments.
- `src/menu/actions.c` (stock, edited) — held-direction repeat is timed by the
  clock (266 ms, then every 30 ms) instead of counting frames, so it is the
  same at any frame rate. The idea of retuning repeat for 60 came from
  N64ever; the code is ours.
- `src/menu/play_stats.c/.h` — times started and time of last start per game,
  keyed by a hash of the full path (a renamed file starts from zero). Up to
  512 games, 6 KB of static memory. Saved to `sd:/menu/playstats.txt` (one
  line per game: hash, count, time; temp file + `safe_file_replace`) at each
  launch, by a one-line hook beside the stock history call in `load_rom.c`.
  Feature `play_stats` (default on), "Count Plays" in Settings > Library.
  64DD disk launches are not counted yet. The path is not profile-aware.
- `src/menu/controls.c/.h` — button layout for the three tabbed screens:
  L/R switch tabs, Z is Options (was R), left/right scroll the carousel,
  up/down do nothing there unless the `updown_scroll` feature is on. It
  rewrites the action flags so the stock screen code is untouched (one-line
  hooks in `browser.c` and `history_favorites.c`). Also tells a tap of A from
  a hold (500 ms) for `quick_launch` (tap starts, hold = info) and
  `hold_launch` (tap = info, hold starts); `load_rom.c` has a one-line hook
  that starts the game when asked. Browser Options has a "Game info" entry.
  `controls_rom_hint()` gives the hint-bar text for the launch mode in use.
  The two launch modes exclude each other: when one ends up on (set to On,
  or set to Default while the theme's default is On) the other is switched
  off, and `features_enabled()` lets Quick Launch win if a theme asks for
  both. Settings shows a row on Default as what it actually comes out as.
- `src/menu/theme.c/.h` — reads `sd:/menu/theme/theme.ini` (or `theme.txt`),
  falls back to built-in defaults. Builds the gradient + pattern background
  once on first draw, at half size in 32-bit color (300 KB, see Memory
  budget); the graphics chip dithers it when it is drawn.
- `src/menu/ui_components/background.c` — shared background now draws the theme
  when no user image is set (a background image set from the image viewer still
  wins), so every screen is themed.
- `src/menu/fonts.c` — `STL_DEFAULT` uses theme `text`, `STL_GRAY` uses
  `text_dim`; other styles keep fixed meanings.
- `src/menu/builtin_themes.c/.h` — four summer themes that come with the
  menu (done and tested): Sunset, Night Drive, Beach, Ocean. Option `theme` in `options.ini`, "Theme" at the top of
  Settings > Display: From SD Card (the default: the player's `theme.ini`,
  or Sunset when there is none), then the four. **Sunset is now the stock
  look**; the old "Midnight Gradient" defaults in `theme.c` only supply the
  frame colors. Safe mode shows Sunset. A change applies at once:
  `theme_reload()` (background rebuilt on the next draw) and
  `fonts_restyle()` (body, title and small fonts take the new text
  colors). Built-in themes set no features. Backgrounds are kept dark on
  purpose, since some text sits straight on them. Checked only as rough
  PC previews. Tested on hardware 2026-10-08: passed; the user then had
  Pool replaced by **Ocean** (built, awaiting test): a new background type
  `ocean` (`THEME_BG_OCEAN`, also `type = ocean` in theme.ini: color1
  water, color2 foam, color3 sky) drawn by sums in `theme.c`: water running
  to a horizon a tenth of the way down, with a net of curved foam lines
  (borders between scattered points, bent with a sine) fading into haze.
  Tested on hardware: passed, slightly speckled far off (the user is
  fine with that), but about 1.4 s to build against 0.3 s for a
  gradient (measured: Sunset 295 ms, Night Drive 302, Beach 236, Ocean
  1358-1373). Skipping far cells and a sine table made no real
  difference. Second attempt (awaiting test): the foam is worked out once
  for a 96x96 square covering 4x4 cells that repeats, stored as "distance
  from a border" in one byte each (9 KB, freed after the build), and each
  screen pixel blends its four nearest values and applies the line
  threshold, which keeps the lines crisp when stretched. Tested on hardware
  2026-10-08: **621 ms** (was 1358), water looks the same. The log prints `theme: background "..." built in N ms`. The
  Theme
  Maker does not know this type yet. Ideas put off to v0.4 by the user (2026-10-09), with the other theme work: a banded sun in the
  intro, a "horizon grid" pattern, palm silhouettes, a time-of-day look,
  summer sounds.
- `src/menu/baked_art.c/.h` and `assets/boxart/` — box art baked into the
  menu file (built 2026-10-08 at the user's request, to cure the slow frame
  when a cover starts loading; done and tested with the user's art). The user
  copies their SD card's `menu/metadata` folder to `assets/boxart/source/`
  (ignored by git); `python3 assets/boxart/make_baked_art.py` shrinks each
  `boxart_front.png` / `boxart_back.png` to fit 158x158 and writes
  `filesystem/art/NZSE_f.sprite` / `_b` (RGBA16, compressed; also ignored),
  which the Makefile packs into the ROM. `carousel_art.c` asks
  `baked_art_load()` first and falls back to the PNG on the card; baked
  covers are freed through `baked_art_free()`. The loader checks the file
  exists and 160 KB is free first, because `sprite_load` halts the menu on
  failure. Nothing is baked by default and no art is in the repository.
  The pipeline was checked with two made-up covers. The game's header is
  still read from the card to learn its code. Only the cover rows use
  baked art; the Game info screen still reads the PNG. A cover is 158x112
  (35 KB in RAM), so the earlier "25-30 MB for the library" guess was far
  too high for one cover, but the user's pack turned out to be the whole
  library: 750 fronts and 296 backs, **1,046 pictures, 24.2 MB baked,
  menu file 27.9 MB** (was 2.5). Files are sorted into
  `filesystem/art/<2nd letter>/<3rd letter>/` so no ROM folder is long to
  walk. The log prints `Baked art: <file> in N ms`. **Tested on hardware
  2026-10-08:** a baked cover loads in 4-7 ms. Stepping quickly through
  the row now averages 35-38 ms with worst frames of 55-70 ms (before:
  40-53 ms and 90-126 ms). A game without a baked cover (NGFE) fell back
  to the card correctly. What is left of the slow frame is reading each
  game's header from the card and the badge lookup. With the
  27.9 MB menu file the user measures **3 seconds from power-on to the
  intro** (2026-10-09), about a second of which is the Ocean background
  being built; scrolling at a natural pace gives 35-38 ms averages and
  worst frames of 55-75 ms. Open: Ocean's build time has crept up with no
  change to its code (621, 749, 930, 944 ms across builds); unexplained,
  possibly its 9 KB lookup square no longer sitting well in the 8 KB data
  cache. If baking does not work out, the
  agreed fallback is one cover file on the SD card with an index.
- `src/menu/game_index.c/.h` — a remembered list of which file is which
  game (done and tested 2026-10-09): the game's code and its
  badge facts, noted the first time a game is seen and saved to
  `sd:/menu/gameindex.txt` (a header line, then one line per game: path
  hash, file size, code, players, flags; temp file + `safe_file_replace`).
  Up to 768 games, 12 KB of static memory. A file is recognised by its
  full path and, when known, its size. `carousel_art.c` asks it before
  reading a ROM's header, `game_facts.c` before looking a game up; both
  note what they learn. Saved when the covers are left
  (`carousel_art_reset()`) and after the selection has rested 3 s, only
  if something was added. Homebrew (found by title) is not noted. The
  save-file check and the favorite check are still done live. This is the
  small first form of the v0.2 "metadata index". **Tested on hardware
  2026-10-09: works** (78 games saved, then remembered after a restart;
  badges correct), but it helped less than hoped: stepping through the
  row gave 38-44 ms averages and 65-77 ms worst frames on the learning
  pass, 36-40 ms and 59-67 ms on the remembered pass. A worst frame of
  about 66 ms is exactly two frames: some frame's work runs just over the
  33 ms budget and waits for the next refresh. So the header read was not
  the main cost left. Timings were added to find it (`cover: starting one
  took N ms`, `badges: lookup took N ms`); measured 2026-10-09: starting a
  baked cover costs 6-9 ms and the badge lookup 13-18 ms for a game that
  saves (0 ms for one that does not), so the save-file check was the
  larger part. Next build (awaiting test): the saves folder is read once
  and its file names noted (`save_exists()` in `game_facts.c`, up to 512
  names, read again after the covers are left), and the Games screen does
  less every frame so a cover start fits in the budget: hint word widths
  are remembered and the title's layout is kept until the selection
  changes (`games_ui.c`). **Tested 2026-10-09:** the badge lookup is now
  0-1 ms (one `saves folder read` of 20 ms per visit), stepping through
  the row averages 36-37 ms, and frames with no cover starting hold 33.4
  ms; but every cover start (6-9 ms) still doubles its frame (58-60 ms).
  Next (awaiting test): cover art is fetched after the frame has been
  handed to the graphics chip (`carousel_art_update()` moved from inside
  `carousel_draw()` to after `draw()` in `view_browser_display()`, and
  `cover_row_after_draw()` for Recent and Favorites), so the chip draws
  while the CPU loads instead of waiting for the rest of its commands.
  The user's next message pasted the previous log again (identical line
  for line), so **this change has not been measured yet**. Same build,
  approved by the user: while covers are coming from baked art the waits
  before loading are 100 ms for the selected cover and 250 ms for the
  sides (`BAKED_SETTLE_TIME_MS`, `BAKED_SIDE_SETTLE_TIME_MS`); art read
  from the card keeps 250 and 700 ms. **Tested 2026-10-09 (a fresh log
  this time):** the user finds it responsive ("could hardly keep up with
  the loading") and saw no downside from the short waits. With many more
  covers loading per second, stepping through the row averages 33.4-36
  ms; some windows with loads now have worst frames of only 41-43 ms, so
  moving the load helped, but most loads still double their frame
  (62-66 ms). Committed. Also seen: flipping a game with no baked back
  looks on the card for one each time (`Boxart: Using path` then the
  baked front again, about 17 ms). Badges for a
  game the list already knows appear after 100 ms instead of 350
  (`KNOWN_SETTLE_TIME_MS` in `game_facts.c`), at the user's request
  (tested 2026-10-09: passed; lookups 0-1 ms, row averages 33.4-37 ms). The log prints
  `game index: N games remembered` and `game index: saved N games`.
  Same build: the Ocean background is drawn flat (seen from above, 160 px
  cells, no horizon or sky) at the user's request, with its lookup square
  cut to 64x64 (4 KB) so it fits the console's 8 KB data cache. Tested:
  **427 ms** (the horizon version had crept to 944). The user asked for
  it finer; 128 px cells were tried and only looked more pixelated (the
  background is built at half size), so it is back to 160 px cells.
- `src/menu/menu_features.c/.h` — Expansion Pak detection
  (`is_memory_expanded()`) and feature toggles. Named `menu_features` to avoid
  clashing with the system `features.h`.

## Status
- Done and tested on hardware: step 1 (theme loader), step 2 (text colors),
  step 3a parts 1-2 (features + `theme.txt`, hollow ring, see-through side
  covers; `8MB` readout confirmed, counter still shows it temporarily).
- **Done and tested on hardware: step 3a part 3 (console side).**
  `constants.h` frame colors now read from the theme: `border`, `highlight`,
  `tab_active`, `tab_inactive`, `tab_active_border`, `tab_inactive_border`.
  Defaults match the stock menu.
- Web Theme Maker updated (2026-10-05): frame/tab color fields, a "Menu
  features" section that writes `[features]`, a preview of the current layout
  (Files and History screens) and v2 share codes. It is a claude.ai artifact,
  not in this repo: https://claude.ai/artifact/HxMPCpbZTSRZURJfWswAma
  The v2 code logic was not run before publishing (no JS runtime in the dev
  container); the user is checking it in the browser.
- Done and tested on hardware: features `side_covers` (default off) and
  `frame_borders` (default on), settable in theme `[features]`. The user
  dislikes both on their CRT but wants others to be able to enable them; side
  covers also sit too close to the overscan edge.
- Done and tested on hardware: step 3b. Settings > "Menu Features"
  (`src/menu/views/features_menu.c`) offers Profile Default / On / Off per
  feature. Choices are saved to `sd:/menu/features.ini` (temp file + rename)
  and override the theme. "Profile Default" (user's wording) means "no
  override, follow the theme / built-in default". Only features that already do something are listed.
  The Settings summary text does not show feature states yet. `side_covers`
  is labelled "Previous/Next Covers" (user's choice).
- Done and tested on hardware: new controls (`controls.c`) and the
  `updown_scroll` feature; the temporary `4MB`/`8MB` readout is removed (the
  user may bring it back later as an optional display).
- Done and tested on hardware: quick launch and hold-to-launch (hybrid).
- Done and tested on hardware: box art on the center cover (US/EU-shaped
  art). Not yet tested: Japanese (tall) and 64DD-shaped art — the user will
  check these before publishing. Box art lives in `sd:/menu/metadata/`; the
  old flat `sd:/menu/boxart/XXXX.png` style is compiled out upstream.
- Done and tested on hardware: safe mode.
- Done and tested on hardware: friendly crash screen (tested with a temporary
  "crash now" menu entry, since removed).
- Done and tested on hardware: launch-mode hint text and the quick/hold
  exclusion. The Files hint bar is tight: the middle hint was cut down to
  `C: Fast | L Tabs R` to clear the longest left hint.
- Done and tested on hardware: new settings screen (`settings_menu.c`),
  including Display > "Video Output" (480i / 240p). That row exposes the stock
  `force_progressive_scan` setting (640x480 buffer shown non-interlaced, so
  the picture is scaled to 240 lines); it applies after a restart. A native
  320x240 layout is still the separate "240p mode" item in v0.5.
- Done and tested on hardware: `debug_stats.c` (free RAM and frame time in
  the debug log). The memory budget table now holds measured 8MB numbers and
  a worked-out 4MB column. Open: confirm on a real 4MB run, fix the safe mode
  + custom background risk, and free memory before v0.2's cover features.
- Done and tested on hardware: cover slide animation (the user found the
  160 ms timing and the text pop natural).
- Done and tested on hardware: box flip (up/down shows the back of the box).
- Done and tested on hardware: art on the previous/next covers (cover
  cache). Cost with side covers on: up to 4 x 35 KB.
- Done and tested (console and Theme Maker): feature `see_through_covers`
  (default on; off draws the side covers solid), "See-through Side Covers" in
  Settings > Display. The Theme Maker (version 6) offers it and
  `carousel_animation`, using share code bits 6 and 7.
- Done and tested on hardware: remember the selected game per folder.
- Done and tested on hardware: sort options (Settings > Files > Sort By),
  and saving the folder position after the selection rests for 3 s.
- Done and tested on hardware: `safe_file.c` (the rename fix). The log shows
  no `could not replace` lines, and after one save of each file the next boot
  no longer reports `features.ini` / `options.ini` / `folders.ini` as missing.
- Done and tested on hardware: Settings saves once on leaving (or after 5 s
  idle) instead of on every press.
- Done and tested on hardware: `hide_extensions`, `tidy_titles` and
  `hide_tags` ("Hide Game Extensions", "Tidy Game Titles", "Hide Region
  Tags").
- Done and tested on hardware: Settings fixes: Library split out of Files
  (the Files page had grown to 11 rows and ran into the description) and
  "Remember Settings Page". Row scrolling is built but has never been
  exercised, because no page currently has more than eight rows.
- Done and tested on hardware: tidied names on History, Favorites and the
  game/disk info screens, with the real-name toggle.
- **`/dur` empties the menu file on the SD card (confirmed 2026-10-06).**
  After a game was launched, a reset gave a black screen: `sc64menu.n64` on
  the card was 0 bytes. With the file copied back by card reader, launching a
  game and resetting works. The fork's menu code is not at fault; the stock
  USB `send-file` path (`usb_comm.c`, used by `localdeploy.bat /dur`) empties
  the file and does not refill it, and the log's `Debug data write dropped
  due to timeout` line is the sign. Why the transfer fails is not yet known.
  It went unnoticed because the cart is powered through the USB cable and
  keeps the uploaded menu in memory across console power cycles.
  **Rules:** do not use `/dur`. Use `deploy-sd.bat` to put a build on the
  card (or plain `localdeploy.bat` to run one from the cart's memory only). A "survived a power cycle" result
  only counts if the USB cable was unplugged or the thing tested lives in a
  settings file.
- Done and tested on hardware: the Font setting (both fonts shipped), and
  with it the Settings row scrolling (Display has nine rows).
- Done and tested on hardware: scroll bar on Settings pages that scroll.
- Done and tested on hardware: the font row renamed "Character Set" and
  moved to System; the ▲/▼ marks removed.
- Done and tested on hardware: menu redesign stage 1 (Games screen chrome).
  The first photo showed the tab bar too close to the top-left corner, so it
  sits 16 px inside the usual safe area.
- Done and tested on hardware: menu redesign stage 2 (badges). Measured
  cost: the frame in which a game is looked up takes 140-165 ms with the log
  attached, against about 80 ms for a cover alone.
- Done and tested on hardware: lighter game lookup for the badges.
- Done and tested on hardware: menu redesign stage 3 (button hints), after
  four rounds of spacing changes from the user's photos.
- Done and tested on hardware: title fonts (26 px and 20 px), used so far
  for the name in the Games title panel (stage 4, first part). The fit check
  leaves 16 px spare, because the text drawer counts the space after the
  last character and otherwise cuts the name with "...".
- Done and tested on hardware: menu redesign stage 4 (Game info screen).
- **60 frames a second: tried 2026-10-06/07, not reachable yet.** The cap can
  be lifted, but no screen finishes its work in the 16.7 ms that 60 needs, so
  each one falls back to 30. Work per frame (8MB, 240p, small font, measured
  with a temporary figure that waits for the graphics chip each frame):
  Settings 17-20 ms depending on the page, Games with one cover about 19 ms,
  Games with five covers about 23 ms, Game info 18.6 ms.
  What was tried: drawing the full-size background in copy mode instead of
  the stretched half-size one made no measurable difference (Settings 17.0
  against 17.6 ms), so that change was backed out and the background stays
  half-size (300 KB) on every console. Skipping the theme background under
  the Game info backdrop did help (24 to 18.6 ms) and was kept.
  What the numbers point to: cost rises with the amount of text on screen
  (Settings pages with more rows cost more) and by about 1 ms per extra
  cover. Text is laid out again every frame. Reaching 60 would mean laying
  text out once and reusing it, which is a project of its own and not started.
  Kept from this work: the clock-based button repeat, the Game info saving,
  and `frame_rate.c` as a hidden option. The Settings row and the measurement
  were removed.
- The 60 fps code is kept on purpose for future experiments (the user's
  request, 2026-10-07): `frame_rate.c` and the hidden option.
- Done and tested on hardware: menu redesign stage 5 (play counts).
- Done and tested on hardware: menu redesign stage 7 (intro), including
  the power-on check by a mark in the cart's memory.
- Done and tested on hardware since then (2026-10-08/09), detailed under
  "What we've changed so far" and "Menu redesign": the tab redesign
  (Games shows games only, Folders tab, player-chosen tab order, Recent and
  Favorites as cover rows), button icons, memory badge, favorite heart,
  rounded corners, the intro's spinning logo and title picture, built-in
  summer themes with the Ocean background, baked box art, the game list
  and the scrolling speed work.
- **v0.1 user-facing features are complete.** The user asked to finish
  v0.1 on 2026-10-10. Its two dev-tooling items (built that day):
  - **Performance overlay** (done and tested 2026-10-10): option `perf_overlay` ("Performance Overlay", Settings >
    System, Off by default): a dark strip at the bottom of the screen
    (y 434-456, below the button hints) with the frame time and worst
    frame, the graphics chip's time and worst, and free memory, over the
    last half second (`debug_stats_overlay_draw()` in `debug_stats.c`).
    Drawn on Games/Folders, Recent/Favorites, Game info and Settings (one
    hook line before each `rdpq_detach_show()`); other screens do not
    show it. The roadmap said "hidden"; it is a normal Settings row so it
    can be switched on without editing files on the card.
  - **PC-side tests** (done, run in the dev container): `tests/`, run with
    `sh tests/run.sh`. The theme reader moved out of `theme.c` into
    `theme_parse.c/.h` (no change in behaviour) so the PC can build it
    with a stand-in `tests/stubs/libdragon.h`; 50 checks. The share code
    test takes the code straight out of `theme-maker.html` and runs it in
    Node.js (installed in the dev container: `sudo apt-get install -y
    nodejs`); 183 checks, including every single-character typo. Both were
    checked by breaking the code on purpose in a scratch copy. Noticed:
    the Theme Maker's decoder holds pattern size to 4-64 and strength to
    0-60 (its sliders' ranges), while the encoder and the console accept
    2-128 and 0-100; harmless, since the Maker only writes values from its
    sliders.
  Left before publishing: the user's test of Japanese (tall) and
  64DD-shaped cover art.
  - **Covers in their own shape (done and tested 2026-10-10).** The user's photo of a Japanese (tall) cover
    showed black bars either side. Each cover's box now takes its art's
    shape (`carousel_art_aspect()`, `carousel_fit_to_art()` in
    `browser.c`), and the ring follows the selected cover's shape,
    easing between wide and tall over 180 ms (`carousel_ring_size()`).
    Before a cover's art arrives it keeps the usual wide shape. The Game
    info cover and its ring are fitted the same way (`draw_cover()` in
    `game_info_ui.c`). Also asked for: a whole-screen swipe when
    moving to and from the Folders tab. Put off by the user (2026-10-10)
    until it fits the memory budget: sliding the background too needs a
    second full-screen copy (about 600 KB), too much for 4MB; sliding
    only what is below the tab bar would need no extra memory.
  - **Games from several folders (done and tested 2026-10-10).** The user's Japanese game and 64DD disks did
    not show on the Games tab, which read the start folder only. The
    user chose (from offered options) a folder list over scanning the
    whole card, and a filter button over separate tabs; the filter is the
    next build. `games_folders.c/.h`: the start folder plus up to 8 added
    folders, kept in `sd:/menu/gamefolders.txt` (one path per line, temp
    file + `safe_file_replace`), added and removed from the Folders tab's
    Z menu ("Add this folder to Games" / "Remove this folder from
    Games", with a two-second note), counted and cleared in Settings >
    Library ("Game Folders", "Forget Added Folders"). **How it works:**
    the Games tab now lists from the top of the card (`sd:/`), and each
    entry's name carries its folder ("N64(JP)/Game.z64"), so every stock
    path built as directory + name still works (launching, history,
    favorites, the game index keys are the same full paths as before).
    `load_directory()` in `browser.c` was split: `scan_folder()` reads one
    folder with a name prefix; the Games tab calls it per folder (a
    missing folder is skipped). `entry_file_name()` gives the part after
    the folder; used by sorting (stock `compare_entry` and
    `sort_order.c`), the position letter and `display_name_file()`.
    Recently Played matches history paths against the list's own names.
    `folder_memory_use_key("games:")` gives the Games tab its own saved
    place (so it no longer shares the root's entry with the Folders tab;
    the old place is lost once). "Set current directory as default" now
    only works on the Folders tab (on Games the directory is the root).

## theme.ini format
```ini
[theme]
name = Midnight Gradient
author =

[colors]
text = E9EDF2
text_dim = A7B0BB
accent = F2B134
panel = 1B2028
border = FFFFFF              ; frame lines (optional, stock values shown)
highlight = 7F7F7F           ; selected row in lists and context menus
tab_active = 6F6F6F
tab_inactive = 3F3F3F
tab_active_border = FFFFFF
tab_inactive_border = 5F5F5F

[background]
type = gradient        ; solid | gradient | image | ocean (color1 water,
                       ; color2 foam; seen from above)
color1 = 1B2A4A
color2 = 0B0E14
color3 = none          ; optional middle stop
direction = vertical   ; vertical | horizontal | diagonal | radial
dither = 1
image = background.png ; only for type = image (not decoded yet)

[pattern]
style = none           ; none | stripes | diagonal | checker | dots | grid | scanlines
color = FFFFFF
size = 16
opacity = 10           ; 0-100

[features]
quick_launch = 1       ; any key from menu_features.c: quick_launch,
                       ; hold_launch, side_covers, frame_borders,
                       ; updown_scroll, cover_art, carousel_animation,
                       ; see_through_covers, remember_selection,
                       ; hide_extensions, tidy_titles, hide_tags,
                       ; play_stats, memory_badge, button_icons,
                       ; favorite_heart, rounded_corners
                       ; (0 or 1; leave out = default)
```

## Share codes (v1, used by the web Theme Maker)
32 bytes, Crockford base32, shown as `SS64-XXXX-...`.
[0] version=1; [1..24] colors RGB: text, text_dim, accent, panel, color1,
color2, color3, pattern color; [25] bits0-1 bg type, bits2-3 direction,
bit4 color3 on, bit5 dither; [26] pattern; [27] size; [28] opacity;
[29] reserved; [30..31] CRC-16/CCITT-FALSE of bytes 0..29.

## Share codes (v2)
52 bytes (84 characters). Bytes 1..28 as v1. [0] version=2; [29..46] colors
RGB: border, highlight, tab_active, tab_inactive, tab_active_border,
tab_inactive_border; [47] feature "is set" mask; [48] feature values, same bit
order: 0 quick_launch, 1 hold_launch, 2 side_covers, 3 frame_borders,
4 updown_scroll, 5 cover_art, 6 carousel_animation, 7 see_through_covers
(bits 6-7 added 2026-10-05; older v2 codes leave them unset); [49] reserved; [50..51] CRC-16/CCITT-FALSE of
bytes 0..49.

## Share codes (v3)
55 bytes (88 characters). Bytes 1..46 as v2. [0] version=3; [47..48] feature
"is set" mask, 16 bits ([47] = bits 0-7, [48] = bits 8-15); [49..50] feature
values, same layout; bits 0-7 as v2, then 8 hide_extensions, 9 tidy_titles,
10 hide_tags (11-15 free); [51..52] reserved; [53..54] CRC-16/CCITT-FALSE of
bytes 0..52.

The Theme Maker writes the shortest version that can hold the theme: v1 when
the frame colors are stock and no feature is set, v2 unless a feature from
bit 8 up is set, otherwise v3. All three decode. A code never starts with
"S" (its first character comes from the version byte), which is how the
optional `SS64` prefix is told apart from an 88-character v3 code.

## Practices (apply throughout)
- Keep a memory budget table in CLAUDE.md (screen buffers, code, covers,
  fonts, audio) and check each new feature against it.
- Print free RAM and frame time with debugf on every hardware test.

## Memory budget (4MB console = 4096 KB)
Measured 2026-10-05 on the user's console **with the Expansion Pak** (8MB,
240p output) from the `stats:` debug lines. Updated the same day after the
theme background went from 600 KB to 300 KB.
The 4MB column is worked out from those numbers, not yet run on a 4MB
console: the same allocations are assumed, with 4096 KB fewer to give.

| What | KB | Basis |
|---|---:|---|
| Outside the heap: program (930) + stack and startup | 974 | measured (8192 - heap 7218) |
| Screen buffers: 2 x 640x480, 16-bit | 1200 | calculated |
| Background: theme gradient (300), or the user's picture (600) | 300 | calculated; 305 KB drop measured |
| Everything else at rest: font, audio, graphics queues, file list, settings | 969 | measured (2469 used - 1500), not broken down |
| **Files screen, no cover loaded** | **3443** | measured (974 + 2469) |
| Center cover (US/EU art) | 35 | measured |
| Extra while a cover is decoding | 54+ | measured at 2 s samples; the true peak may be higher |
| **Files screen, cover loading (worst seen)** | **3532** | worked out (cover numbers measured before the change) |

| On a 4MB console (heap 3122 KB) | Free KB |
|---|---:|
| Files screen, no cover | 653 |
| Files screen, cover shown | 618 |
| Files screen, while a cover decodes | 564 or less |
| Settings screen | 653 |
| Game info screen with its box art | 618 |

What this means:
- The first estimate was about 110 KB too optimistic (420 free guessed, 309
  projected). The "everything else" bucket is 978 KB, not the 745 guessed.
- Reserve rule: keep at least 256 KB free on the Files screen on 4MB. With
  about 564 KB free at the worst moment there is roughly 300 KB to spend,
  which is about six more cover-sized images. That is the budget for v0.2's
  cover cache, art on previous/next covers and the slide animation.
- With a user background picture (600 KB) instead of the theme background,
  subtract 300 KB from every "free" figure: about 264 KB at the worst moment,
  i.e. nothing to spare. Cover extras must cope with that case.
- A full-screen 16-bit image costs 600 KB; a second one does not fit.
- Theme background: built at 320x240 in 32-bit color (`BG_SCALE` in
  `theme.c`), stretched 2x when drawn, dithered by the RDP at full resolution
  (`DITHER_BAYER_NONE`). A full-size copy-mode version was tried again on
  2026-10-07 for speed and made no measurable difference, so it was dropped. Tried first as 16-bit with baked dither (150 KB): the
  dither looked coarse on the CRT. The 32-bit version looks almost identical
  to the original full-size one, frame time unchanged (33.1-33.4 ms). The
  user prefers saving RAM over keeping the full-size background.
  `dither = 0` themes are not yet checked with this version.
- Remaining candidates for freeing memory: break down the 969 KB bucket (the
  font is stored compressed, so it may be well over its 465 KB file size);
  fewer or smaller audio buffers.
- Program size grows with every feature; re-measure when updating the table.

Frame time (30 fps cap, so 33.3 ms is on target): steady 33.4 ms on every
screen. One frame of about 50 ms roughly every 6 s on the Files and game info
screens (cause unknown). Loading a cover costs one hitch of 70-75 ms. Opening
the first folder took 1.4 s.

**The 50 ms frame every 6 s: cause found and fixed 2026-10-09 (the user's
log confirms it: resting worst frames are 35-38 ms on every line).** The cap was `display_set_fps_limit(30)`,
but an NTSC console refreshes 59.83 times a second, so every second refresh
is 29.91. libdragon's limiter keeps a running sum to hit 30, and every 5.7 s
it accepts two refreshes in a row and from then on uses the other half of
them; the frame in progress cannot be ready 16.7 ms early, so it waits three
refreshes (50 ms). `frame_rate.c` now asks for exactly half the refresh rate
(on PAL a steady 25 instead of an uneven 30). Same build, for finding why a
6-9 ms cover load doubles its frame: the `stats:` line also gives `work avg /
worst` (processor time per frame, from getting a buffer to the end of the
screen's drawing and loading; hook `debug_stats_begin()` in `menu.c`), and
any frame over 45 ms prints `slow frame: N ms (work N ms, the frame before
N ms)`.
What that showed (2026-10-09): at rest the Games screen's work is 23 ms a
frame. A cover load adds 8-9 ms of work and no longer doubles the frame
(worst 41-44 ms as measured, which is the same frame arriving on time).
The slow frames left (58-67 ms) have ordinary work, 22-25 ms: the processor
was done and the frame still missed its refresh, so the graphics chip is
what runs late, mostly while covers are sliding. The 2026-10-07 finding
that a cheaper background "made no difference" was measured when the
processor was the slower of the two, so it says nothing about the chip.
That reading was wrong, and the measuring build showed it (2026-10-09):
with the log waiting for the chip on every frame (`rspq_wait()`), the slow
frames vanished: stepping through the row gave worst frames of 42-44 ms
(a cover load's extra work, on time) and the chip finished 1.5-3 ms after
the work, 23-24 ms into the frame. **Real cause:** `rdpq_detach_show()`
puts the finished frame on screen through a "deferred call", and libdragon
only runs those when some code next talks to the chip. Between frames the
menu idles, so that was whenever the sound code needed the chip, sometimes
after the refresh. **Fix (tested 2026-10-09: stepping through the row gives
no slow frames, worst 41-44 ms; committed):**
`frame_rate_end_frame()` in `frame_rate.c`, one hook in the `menu.c` loop,
waits for the chip after every frame. The `stats:` line keeps `work` and
`chip` (start of work until the chip is done; over 33 ms is a missed
refresh). Measured at rest: Settings work 14 / chip 16 ms; Games 16.3 /
18.3 with one cover, 21.5 / 23.2 with five. Still slow and genuinely the
chip: the fade-in (the full-screen see-through black costs it about 11 ms:
work 21, chip 34, so the fade runs at 15 frames a second), and one frame
of 60-80 ms whenever a settings file is saved.
Also measured: the Folders tab's work is 29 ms a frame (the Games tab's
is 21-24), the highest of any screen, so it has the least room.
**Covers after a game without baked art (done and tested 2026-10-09):** the user saw the cover after such a game arrive late.
Two causes in `carousel_art.c`: one cover from the card put the following
covers back on the long waits (250/700 ms), and a PNG still decoding held
up every other cover, though baked art does not use the decoder. Now the
short waits stay once any baked cover has loaded, and baked covers load
while a card picture decodes (`card_pending` marks a cover whose card
picture waits for the decoder).

Fixed (built, awaiting hardware test): safe mode no longer loads the custom
background picture at all (`load_from_cache()` in `background.c`), so it
can't sit in RAM next to the theme background.

Image viewer on 4MB (tested on 8MB 2026-10-05: used RAM stays at 2774 KB on
the Files screen and inside the viewer): the theme background
(600 KB) used to stay in RAM while the image viewer decoded a full-screen
picture, which could not fit in the free memory on 4MB. The stock viewer
already frees the user's background picture first; `theme_background_suspend()`
/ `theme_background_resume()` now do the same for the theme background, hooked
into the three stock functions in `background.c` that the viewer calls. The
loading screen shows a plain color meanwhile, and the gradient is rebuilt on
return. That took about 1.25 s at full size; the half-size background made
it visibly faster. The user has a Jumper Pak on the way, so real 4MB runs become
possible; until then everything 4MB-specific is worked out, not tested.

Re-measured 2026-10-06 (8MB): the heap is 7198 KB, so the program has grown
by about 20 KB since the table was made (outside the heap: 994 KB; take 20 KB
off every 4MB "free" figure). Files screen baseline still 2469 KB used; with
Previous/Next Covers on and all five covers loaded, 2651 KB (+182 KB, about
36 KB per cover). Saving a settings file costs one frame of 60-85 ms; changes
in the Settings screen save on every press, with one outlier of 440 ms.

Breakdown of the Files screen baseline (2469 KB used), 2026-10-06:
| What | KB | Basis |
|---|---:|---|
| Screen buffers: **two** x 640x480 16-bit (`display_init(..., 2, ...)`) | 1200 | calculated |
| Theme background, 320x240 32-bit | 300 | calculated, drop measured |
| Font `Firple-Bold.font64` | about 760 | built once without compression: 778,424 bytes. Assumes it sits in RAM uncompressed. Charset is 7,799 characters (includes Japanese). |
| Everything else: audio buffers and mixer, graphics queues, file list, settings, history | about 209 | by subtraction, not broken down further |

The font is the largest single item after the screen buffers. Its charset
has 2,650 characters: 339 Latin ones (ASCII, Latin-1, Latin Extended, the
arrows and ellipsis the menu uses), 171 kana and CJK symbols, and 2,140 kanji.
Measured 2026-10-06 by building the font in a scratch folder with the same
settings (not added to the build):

| Font | In RAM (uncompressed file) | In the ROM (compressed) |
|---|---:|---:|
| Full, as shipped | 778,424 bytes (760 KB) | 476,559 bytes |
| Latin only, 339 characters | 65,336 bytes (64 KB) | 28,935 bytes |

So a Latin-only font would free about 696 KB of RAM, taking the worked-out
free memory on a 4MB console from about 633 KB to about 1,330 KB. Shipping it
beside the full font adds 29 KB to the ROM. The price: Japanese file names
would show missing characters where the small font is used. Not decided and
not built into the project.
**Confirmed on hardware 2026-10-06** with a throwaway build carrying only the
Latin font: "used" on the Files screen fell from 2469 KB to 1759 KB (710 KB
saved), text and the arrow characters looked right, and the ROM shrank from
1.80 MB to 1.36 MB. The full font was put back afterwards.
The menu is already double-buffered, so "use two buffers instead of three"
saves nothing. Drawing the theme background live would now save 300 KB, not
600 KB.

Still to measure: a real 4MB run, a folder with many entries, the image
viewer and the music player.

## Menu redesign (fast-tracked by the user, 2026-10-06)
Working from two mockups the user supplied: a Games screen (pill tabs with
L/R and a clock, cover row, title panel with badges, position bar, button
hints) and a Game info screen (blurred cover art as the background, big
title, badges, "Played / Last played / Save" boxes, description, Play
button). The Games screen is the landing screen (it already is, except that
the very first run shows the credits). A short intro with music comes later.
Stages, one testable build each:
1. **Games screen chrome (done and tested):** tab bar
   (Games / Recent / Favorites, in L/R cycling order) with clock, no frame,
   covers moved up 20 px, title panel, position bar. Hints unchanged. The
   `frame_borders` feature no longer affects these three screens.
2. **Badges on the title panel (done and tested):** players, "Needs
   Expansion Pak" / "Expansion Pak", "Save found", "Favorite". `game_facts.c`
   looks the game up 350 ms after the selection settles. First version used
   the stock `rom_config_load()`: 140-165 ms for that frame, five file opens
   and about fourteen log lines per game. **Lighter version (done and
   tested: 77-99 ms, the same as a cover alone):** `rom_info_load_basic()` (added to `rom_info.c`: header
   and built-in database only), the player count read straight from the
   metadata pack's `metadata.ini`, and the last 32 games remembered so
   revisiting one reads nothing but the save-file check. A per-game settings
   file that overrides the save type is not consulted here.
3. **Button-badge hint bar (done and tested):** on the Games
   tab only: A, Hold (when a launch mode is on), B, Z, C in boxes with one
   word each. One row did not fit in the longest case, so (the user's
   suggestion) the hints are stacked in two rows from y=394: A and Hold on
   top, B and Z below, START Settings and C Fast scroll right-aligned. The
   position bar was narrowed to the middle three quarters of the screen at
   the user's request, and the title panel and hints were then pulled in to
   share edges 48 px inside the safe area (480 px wide, a little wider than
   the position bar). Recent and
   Favorites keep their text hints until stage 6.
4. **Game info screen (done and tested, 33.3 ms):** `game_info_ui.c`
   draws the main page: a tiny copy of the front cover (a quarter size each
   way, about 2 KB, kept while the game is shown) stretched over the whole
   screen with smoothing and drawn at a third of its brightness, the
   cover in an accent ring at top right, the maker, the name in the largest
   title size that fits two lines, badges, three fact boxes (Players, TV
   region, Released; the user asked for the save type to be dropped from this
   page, it remains under Options > Set Save Type), the description with the per-game switches on its last
   line, and two rows of button hints. Hooked into `load_rom.c` with an
   `#if 1 ... #else stock #endif` around the stock main page; the pop-ups and
   all button handling are stock and unchanged. Risk: two full-screen
   stretched pictures per frame (theme background, then the cover), so the
   frame time needs checking.
5. **Launch stats (done and tested):** `play_stats.c`. Game
   info's three boxes are now Played, Last played and Players, as in the
   mockup; the release date moved up beside the maker, and TV region left the
   page (still under Options > Set TV Type).
6. Recent and Favorites as cover rows instead of lists.
   **Recent (built 2026-10-08, awaiting hardware test, not committed):**
   `cover_list.c/.h` is a small layer that says which list the cover row
   shows: the open folder (default) or another list. The cover drawing
   (`carousel_draw()` in `browser.c`, now exported through `carousel.h`),
   the art cache (`carousel_art.c`) and the badge lookup (`game_facts.c`)
   read entries, selection and file paths through it instead of from
   `menu->browser`. `cover_row.c/.h` builds such a list from the history
   (empty places left out), moves along it, draws it with A/Hold hints,
   and remembers the selected cover while the console is on. Hooked into
   `views/history_favorites.c` for the Recent tab only; launching still
   goes through the stock `load_history_id` path. A 64DD entry's second
   file (disk + game pairs) is no longer shown. Favorites still uses the
   list and its text hints; it is the next build.
   Hardware test 2026-10-08: everything passed except that B on a game's
   info screen went to the Games tab (stock behaviour). Fixed in the next
   build (awaiting test): each tab calls `games_ui_set_origin()` when it
   opens and `load_rom.c` / `load_disk.c` go back to `games_ui_origin()`.
   Measured: Recent with three covers uses about 100 KB more than the
   Games screen at rest, back to 1904 KB after leaving; 33.3-33.7 ms.
   Same build: the L and R badges are gone from the tab bar (the user
   finds them clutter), and feature `memory_badge` (default on, "Memory
   Badge" in Settings > Display) draws a small Expansion Pak (red top) or
   Jumper Pak (grey top) under the clock with "8MB" / "4MB". The pak is
   drawn with rectangles, no picture file.
   **Games-only tab, Folders tab, tab order (built 2026-10-08, awaiting
   hardware test, not committed; the user's decisions):**
   - The Games tab shows games only (ROMs, 64DD disks, emulator games), no
     folders, from one folder: the start folder (`default_directory`).
     Later it becomes every game on the card, once the metadata index
     exists (the user chose "one folder now, whole card later").
   - A new Folders tab is the plain file browser: every file by its real
     name with its size, folders in yellow, the path on top
     (`folders_ui.c`). Pictures, music, text and zips are opened from here,
     and "Set current directory as default" in its Options is how the Games
     tab's folder is chosen.
   - Both are the same screen and mode (`MENU_MODE_BROWSER`, so every stock
     "back to the browser" lands on whichever was showing). `browser.c`
     keeps `folders_tab` and the Folders tab's own directory, filters the
     list at load on the Games tab (one hook in `load_directory()`), and
     reloads the list when switching between the two.
   - `tabs.c/.h`: the bar has four places, options `tab1`..`tab4` in
     `options.ini` (None / Games / Favorites / Folders / Recent), set in the
     new Settings category "Tabs". Default: Games, Favorites, Folders; the
     user wants Recent to be something the player adds. Duplicates count
     once; all None falls back to the default. The menu opens on the first
     tab. L/R step through the list on every tabbed screen.
   - Hardware test 2026-10-08 (photo): passed, tab switching "almost
     seamless"; four tabs and the clock fit. Committed as `6b51e292`.
   - Done and tested: START opens the stock START menu on Recent and
     Favorites too (`start_menu.c`, a copy of the browser's list so the
     stock code is left alone), and Settings goes back to the tab it was
     opened from.
   - **Icons (built 2026-10-08, awaiting hardware test, not committed).**
     All drawn from scratch by `assets/images/make_icons.py` (needs
     Pillow: `sudo apt-get install python3-pil`); the product photo the
     user first supplied was tried for one test build and removed without
     ever being committed. The Makefile's new `IMAGES` list builds them
     into sprites in the smallest formats: `expansion_pak` and `jumper_pak`
     (24x26, 16 colors, CI4, about 330 bytes; same shapes and angle, red
     lid with air holes against plain grey; only the one matching the
     console is loaded) and `button` (a 20x20 white disc, I4, 200 bytes).
     Feature `button_icons` (default on, "Button Icons" in Settings >
     Display): the hints show A blue, B green, C yellow as discs and START
     red, Z grey, Hold blue as longer rounded shapes, all made from the one
     disc (tinted with the prim color; long shapes are two half discs and
     a filled middle). New text style `STL_WHITE` for the letters. Off
     gives the old boxes.
   - Same build: pressing START again closes the START menu (one line in
     `browser.c`, and in `start_menu.c`).
   - Icons build tested on hardware and committed (`61831769`).
   - **Built 2026-10-08, awaiting hardware test, not committed:**
     Favorites is a cover row too (`COVER_ROWS` switch in
     `history_favorites.c`; Z removes the selected favorite and the row is
     rebuilt in place). The memory badge has "Detected" in green under
     "8MB"/"4MB", in a new 12 px font (`FNT_SMALL`,
     `Firple-Bold-Small.font64`, English letters only, 7 KB in the ROM,
     about 10 KB in RAM, loaded by `title_font.c`), and is hidden on the
     Folders tab, where it ran into the list. Feature `favorite_heart`
     (default on, "Favorite Heart" in Settings > Display): a pink heart
     (`heart.sprite`, 16x14, I4, 112 bytes) after the game's name on the
     title panel, with room always kept for it so the name never changes
     size; off shows the word "Favorite" as before. 12 px text was
     earlier judged too small for a composite CRT; the user asked for it
     here, so check the photo.
   - Test of that build (2026-10-08, photo): passed except that "8MB"
     and "Detected" did not show (their text boxes were lower than one
     line of the font, and rdpq drops text that does not fit its box:
     **give a text box at least the font's line height**), and Z removed
     a favorite too easily.
   - **Next build (the memory text and the Z confirmation confirmed
     working by the user; the rest awaiting comment; not committed):** that text fixed; Z on
     Favorites asks "Remove from Favorites?" (A removes, B keeps); and
     four visual changes the user approved: the selected tab is a solid
     accent-colored block with white text (black text was tried on light
     accents and ran together into a blob: the font's dark outline plus a
     black fill; **never draw black text with this font**); Tidy Game Titles also turns
     underscores into spaces; a see-through dark band sits behind the
     button hints (`games_ui_hints_backdrop_draw()`); the position bar's
     letter comes from the file name, which is what the list is sorted by.
   - **Intro logo (same build, awaiting test):** `intro_logo.c` draws a
     spinning 3D "N" above the name: four pillars and four slanted bars as
     44 flat-colored, lit quads (88 triangles, no picture file, no memory),
     sorted side by side instead of using a depth buffer (which would cost
     600 KB). First version spun twice and slowed to a stop; the user
     then gave the Ocarina of Time boot logo as a second reference
     (keep our colors, same size), so it now turns steadily (one turn in
     2.6 s), is a little squatter, and looks polished: each face is shaded
     from top to bottom (smooth-shaded triangles) and glints when turned
     toward the light. The user's recording (2026-10-08) showed it
     working but washed out to pale colors and without the reference's
     patterns. Cause of the wash: the glint was aimed almost straight at
     the viewer, so every face looking our way was whitened. Third version
     (awaiting test): the reference picture's own colors (sampled from
     it), the glint cut to a faint one, and two generated patterns on the
     pillars, a marbled "hologram" on two opposite sides and pink-to-blue
     stripes on the other two (`logo_holo.sprite` 32x32 and
     `logo_stripes.sprite` 16x64, RGBA16, 2 KB each, made by
     `make_icons.py`, loaded for the intro only), drawn as textured,
     perspective-correct, shaded triangles. A GIF the user pastes can be
     split into frames with Pillow to look at.
     Fourth version (built, awaiting test, not committed), from a clearer
     reference: one pattern covers each whole side (both pillars and the
     bar across them), the bar's upper edge is red and its lower edge
     grey, the pillars' gap sides teal. A per-side bar color was tried in
     between and dropped. New option `intro_logo` ("Intro Logo" in
     Settings > System): Vaporwave (default) or Classic, the console's own
     green, blue, red and yellow, plain and glossy with no patterns. The
     classic colors and which face gets which were set from memory, not
     from a picture; expect the user to correct them.
     Then, at the user's request: the logo is smaller (52 px half
     width, middle at y=160) and further from the title; and the title
     is a picture, `wordmark.sprite` (448x124, RGBA32 for its soft glow,
     70 KB in the ROM, **about 220 KB in RAM, loaded for the intro only**
     and skipped when less than 700 KB is free, in which case the plain
     text title and accent line are used). `make_wordmark.py` draws it
     in an 80s style after a reference the user gave: "SUMMERSTATION" in
     chunky pink-to-yellow letters with a white rim and a slab of depth,
     "64" in neon handwriting across it. Fonts: Anton and Mr Dafoe, both
     Open Font License, kept with their licences in
     `assets/images/wordmark/`; see `assets/images/CREDITS.md`. Seen by
     the user 2026-10-08: "looks good", to be left as it is for now and
     come back to later.
     **Tested on hardware 2026-10-08 (photo): passed**, along with the
     tab look, hint band, underscores, position letter, "Detected" and
     the remove prompt. The maths was checked on the PC first by drawing it from eight
     angles with a Python copy. **The shape is Nintendo's N64 logo. The
     user decided on 2026-10-08 to use it, knowing that, and will remove it
     if asked; do not raise it again.** The name and accent line moved
     down to make room (name at y=272).
   - **Row wrapping round (done and tested 2026-10-09):** with the stock "Wrap File List" setting on (Settings >
     Controls, default off), the last game is drawn to the left of the
     first and the other way round, its art loads, and Recent and
     Favorites wrap too (they did not before). `cover_list_neighbor()` in
     `cover_list.c` gives the entry a number of places from the selected
     one; a place is only filled by wrapping while twice its distance is
     less than the number of games, so no game shows on both sides.
   - **Ring tint, second version (done and tested 2026-10-10).** The user's photos showed blue covers
     (Perfect Dark, Pilotwings, The New Tetris) getting a red ring or the
     accent: the average of a varied picture is a muddy near-grey (so the
     accent was used), and the red N64 strip on the right of US boxes
     pulled dark covers to red. New method in `art_tint.c`
     (`art_tint_from()`): every second pixel, leaving out the right 22%
     of the box, greys and near-blacks skipped; hues sorted into 12
     slices weighted by how colorful each pixel is; the strongest slice
     (with half its neighbours) wins, and its average color is made
     vivid. Checked first on 16 of the user's covers with a Python copy
     (PD, Pilotwings and Tetris come out blue; Zelda OoT gold, Majora
     orange). The switch became option `ring_tint` ("Tint Ring From Art",
     Settings > Display): Off / Game Info (default) / Everywhere, the
     user's "sub-option": Everywhere also tints the cover rows' ring with
     the selected cover's color (`carousel_art_tint()`, worked out once
     per loaded cover; `carousel_ring_color()` in `browser.c` fades to
     it over 250 ms). The `ring_tint` feature key from the first version
     is gone (no longer in theme.ini's list).
   - **Game info: one details pop-up, and the ring tinted from the art
     (done and tested 2026-10-09).** START
     opens "Game details", the stock "extra" (L) and "advanced" (START)
     pop-ups in one, less what the page already shows (players, release
     date, author); START or B closes it; L does nothing on this screen
     now. The stock pop-up code in `load_rom.c` was replaced in place.
     Feature `ring_tint` (default on, "Tint Ring From Art" in Settings >
     Display): the ring round the Game info cover takes the cover's
     average color, stretched to be vivid (`ring_tint_make()` in
     `game_info_ui.c`, worked out when the backdrop is made, no extra
     memory); a nearly grey cover keeps the accent color. Only the Game
     info ring; the cover rows' ring is still the accent color.
   - **Game info page tidied (done and tested 2026-10-09)**, from a list of suggestions the user brought from
     another Claude conversation, all in `game_info_ui.c`:
     the date reads "Oct 26, 2000"; a game that needs the Expansion Pak
     shows a green "Uses Expansion Pak" when the console has one and the
     orange "Needs Expansion Pak" only when it has none (also on the cover
     rows' title panel, `games_ui.c`); a "Series - Subtitle" name is the
     series on one big line with the subtitle smaller and dimmer under
     it; the description box runs down to the button hints (five lines,
     was three), is laid out once per game, and a description too long
     for it is cut at a word and ends in "..." (`desc_build()`, which
     relies on `rdpq_paragraph_build()` reporting how many bytes fitted:
     not checkable on the PC, so look at the photo); the Cheats / Patches /
     Clear memory line is gone, replaced by orange badges at the bottom
     of the box only for the ones switched on; the button hints sit on
     the same dark band as on the cover rows. From the same list, not
     done: merging "More info" and "Technical" into one button, a more
     even dimming of the backdrop, and tinting the ring from the art
     (box art tinting is a v0.4 item).
   - **Each tab remembers its game (done and tested 2026-10-09).** The user found that switching tabs lost the
     position. Causes: switching between Games and Folders reloads the
     list, and `folder_memory` only restores on arriving in a *different*
     folder, so with both tabs on the same folder nothing was restored;
     and `cover_row.c` remembered one list only, so Recent and Favorites
     overwrote each other. Now `browser.c` keeps the name of the entry
     each of its two tabs was left on (`left_on`) and selects it after the
     reload, and `cover_row.c` keeps a place for each of the two lists.
     All of it lasts while the console is on; `folders.ini` still covers
     the Games tab across a power cycle.
   - **Three items the user asked for together (done and tested
     2026-10-09; the tab slide plays nothing when going to Folders and
     never moves the tab being left):**
     - Covers slide in when a tab is switched to: from the right after R,
       from the left after L, 200 px over 200 ms, easing into place
       (`tabs_slide_begin()` / `tabs_slide()` in `tabs.c`, one hook line at
       each of the two places a tab is switched, and a shift applied in
       `carousel_draw()`). Follows the Cover Slide setting. Covers that
       would be partly off the screen are left out until they are on it.
       The ring, title panel and Folders list do not move.
     - The Game info screen takes its cover from baked art when there is
       some (`baked_art_open()` / `baked_art_release()` in `baked_art.c`;
       `load_rom.c` calls them in place of the stock load and free). Other
       pictures (sides, top) and games without baked art still come from
       the card. The 64DD disk info screen is unchanged.
     - Turning over a box with a baked front and no baked back no longer
       searches the card (17 ms each time): a baked front is taken to mean
       all of that game's art was baked. A back picture added to the card
       later is not seen until the art is baked again.
   - Not done yet: the Games tab has no way to change folder
     except through Folders.
7. **Intro (done and tested):** `intro.c`. The name (`MENU_DISPLAY_NAME`,
   defined in `menu_name.h`) in the 26 px title font over the theme
   background, an accent line growing under it, up from black and back to
   black, 2.6 s, with a tune on sound channel 1. Runs inside the stock
   startup screen (two hook lines in `views/startup.c`). Only after
   power-on, so not when coming back from a game; not in safe mode or with
   autoload. Any button skips. "Intro" in Settings > System. The tune follows the Sound Effects setting. It is our
   own, written by `assets/sounds/make_intro.py` (sine waves, 22 kHz mono;
   29 KB in the ROM, streamed, so no real RAM cost).
   **Telling power-on from RESET:** `sys_reset_type()` is no use, because
   the cart's start-up program always reports a reset (seen in the log). So
   the menu leaves a mark in the last 8 bytes of the cart's 64DD sector
   buffer (0x1FFE28F8) at every start. Confirmed on hardware: the mark
   reads as zeros after power-on and is still there after RESET, also after
   a game has run. SummerCart64 only; other carts play the intro on every
   start. After a 64DD game the mark is overwritten, so the intro plays on
   that RESET. With the USB cable in, the cart never loses power, so the
   intro shows only on the first start after an upload. The log prints the
   mark it read and `intro: power-on start, playing` or why it was skipped.
   **Fade-in (done and tested):** on every start, intro or not, the Games
   screen comes up from black and the background music rises from silence,
   together. 3 s and 2.25 s were tried; the user chose 2 s, with 1 s as a
   choice. The picture brightens fast at first, so the menu is readable
   early. It also hides the short black gap between the intro and the
   Games screen. The music is silent during the intro either way. Hooks:
   `intro_poll()` in the `menu.c` loop, `intro_fade_draw()` at the end of
   the Games screen's `draw()` in `browser.c`.
   **Settings (built 2026-10-07, awaiting hardware test):** at the user's
   request both are three-way choices in Settings > System, saved in
   `options.ini`, so themes cannot set them: `intro` (Off / On = power-on
   only, the default / Both = also after RESET) and `fade` (Off /
   1 Second / 2 Seconds, the default). This replaced the short-lived
   `fade_in` feature and `fade_speed` option; the `boot_animation` feature
   is still in the table but nothing reads it now.
   **Sounds swapped by the user, 2026-10-07 (built, hardware test not
   reported yet):** new `bgm.wav` (38 s, 859 KB in the ROM),
   `settings.wav` (stereo, 0.8 s) and `intro.wav` (stereo, 3.4 s, 152 KB).
   The user is trying out a default set to their taste; sound packs stay in
   v0.4. The ROM is now 2.41 MB (was 1.93). Because of this the intro now
   lasts as long as `intro.wav` (read when it is opened; at least 2.6 s, at
   most 8 s), plays on channels 2-3 so a stereo tune can't collide with a
   stereo sound effect on 0-1, and `make_intro.py` writes
   `intro_generated.wav` so it can't overwrite the user's tune. Sources and
   licences are in `assets/sounds/CREDITS.md` (checked 2026-10-07): the
   intro and settings sounds are CC0 (Lokif, opengameart.org); the music is
   by Eric Matyas (soundimage.org), free to share **with credit shown in
   the product itself**: "Music by Eric Matyas www.soundimage.org". The
   track is "Cyber-dream-loop". That line is on the menu's credits screen
   (`views/credits.c`, in place of one blank line, so the page is no
   longer; built 2026-10-08, awaiting the user's look), and Lokif is named
   in that screen's library pop-up. The user reports the new sounds are
   good on the console.
Rounded corners (built 2026-10-08, awaiting hardware test, not committed):
feature `rounded_corners` (default on, "Rounded Corners" in Settings >
Display). `round_fill()` in `games_ui.c` draws a box whose corners are the
four quarters of one 16x16 white disc (`corner.sprite`, I4, 128 bytes),
tinted, with three plain boxes between them so see-through colors are not
drawn twice. Used for the tabs, the clock, the title panel, the badges and
the band behind the hints. Not rounded: the selection ring, the covers,
Settings, the Folders list. Tested on hardware 2026-10-08: passed; at rest the Games
screen holds 33.3 ms (worst 36) with them on, so they cost nothing that
shows. The same log showed 40-53 ms averages and 90-126 ms worst frames
while stepping quickly through the row with Previous/Next Covers on, the
same with corners off: that is cover loading. Starting a load costs one
slow frame (ROM header read, art path lookups, opening the PNG; the
decode itself is spread out), and with side covers up to five loads start
one after another at each stop. Change (awaiting test): side covers wait
until the selection has rested 700 ms (`SIDE_SETTLE_TIME_MS`), the
selected cover still 250 ms. The real cure is the metadata index and the
asset prep tool (v0.2).

Constraints found so far: rounded corners were first thought too costly on
the N64 (square corners used) until the tinted-disc trick above; small label text from the mockups would not be readable on a
composite CRT; there is one font size (15 px), so a big title needs a second
font. Measured 2026-10-06 (same typeface, uncompressed size = RAM cost):

| Characters | 22 px | 26 px | 30 px |
|---|---:|---:|---:|
| ASCII only (95) | 26 KB | 32 KB | 41 KB |
| ASCII + Western European accents (190) | 56 KB | 69 KB | 88 KB |
| The full small-font set (339) | 110 KB | 140 KB | 177 KB |

Chosen (the recommendation the user accepted): 26 px with accents, 69 KB.
`title_font.c` loads it as `FNT_TITLE` (`Firple-Bold-Title.font64`, built by
a Makefile rule from `assets/fonts/charset-title.txt`). `title_font_pick()`
returns the title font only if it has every character of the text and the
text fits on one line; otherwise the body font, so nothing shows as gaps.
After the user's photo showed long names looking small at 15 px with half
the panel empty, a 20 px size was added (`FNT_TITLE_MEDIUM`,
`Firple-Bold-Title20.font64`, 48 KB): the largest of 26, 20 and 15 px that
fits on one line is used. The two title fonts together cost about 117 KB.

## 4MB test checklist (for when the user says the Jumper Pak is ready)
Everything 4MB-specific so far is worked out from 8MB runs. When the user
says they are ready to test on 4MB, give them this list, updated with
anything added since. Run with the log open (`.\localdeploy.bat /d`, or
`.\deploy-sd.bat /d`) and note the `stats:` lines for each step.

Basics
1. The menu boots, and the log's first Files screen line shows a heap of
   about 3,100 KB.
2. The log says `font: small (Latin)` with Character Set (Settings > System)
   on Auto, and "used" on the Files screen is about 1,760 KB (so about
   1,340 KB free).
3. Set Character Set to Full and restart: "used" rises by about 710 KB and
   the menu still works. Set it back to Auto.
4. Frame time stays near 33 ms on Files, Settings, History and game info.

Covers
5. A cover loads on the selected game; flip it over and back.
6. Previous/Next Covers on: all five covers load, and free memory stays
   above 256 KB while one is decoding.
7. Scroll quickly through a long folder with side covers on: no crash, no
   wrong art.

Backgrounds and pictures
8. Open a full-screen (640x480) PNG in the image viewer. This is the case
   the theme background is freed for.
9. Set that picture as the background, then check the Files screen: with
   Character Set on Full, side covers should fall back to placeholders rather than
   run out of memory. Then remove the background.
10. Safe mode (hold Z) with a custom background set: boots, and "used" is
    not 600 KB higher than normal.

Limits the stock menu applies on 4MB
11. A folder with more than 1,024 entries shows the "too large for Jumper
    Pak" message instead of crashing.
12. A zip with more than 512 entries shows its message.
13. A game that needs the Expansion Pak shows the warning before starting,
    and never starts directly through Quick Launch or Hold A To Launch.

Everything else, once each
14. Settings: every page, including a change that saves.
15. Sort By each order; Remember Position after a restart.
16. Launch a game, reset: the menu returns (USB cable unplugged).
17. The music player and the text viewer open and close.
18. The intro plays at power-on (USB cable unplugged) and not after RESET.
    The log should say whether the title is the picture or text: the
    picture needs 220 KB and is skipped below 700 KB free.
18a. Each built-in theme, Ocean above all (its background takes about
    0.4 s to build on 8MB).
18b. With baked art: covers load, and none is loaded when less than 160 KB
    is free. Note the start-up time with the larger menu file.
18c. The game list: `game index: N games remembered` after a restart.
19. The friendly crash screen has not been seen on 4MB; if anything crashes,
    note what the screen shows.

Also still untested on any console: a theme with `dither = 0`, and Japanese
(tall) and 64DD-shaped cover art.

## Release plan
- **v0.1 usable carousel:** theme loader ✓, text colors ✓, feature toggles +
  Expansion Pak check, safe mode + friendly crash screen, left/right browsing,
  quick launch (skip the ROM info screen), box art on covers.
  - Dev tooling: debug overlay, a hidden toggle showing frame time and free
    RAM.
  - Dev tooling: PC-side automated tests for the theme parser and share code
    decoder.
- **v0.2 smooth and safe:** metadata index, cover cache, perspective covers +
  slide animation, SteamOS-style boot animation, save backups, sort options,
  4MB-friendly live background, asset prep tool.
  - Extra game details in the metadata index: year, developer, publisher,
    genre. Plan the index format with these fields from the start.
  - Verified ROM badges: the asset prep tool (on PC) checks ROMs against known
    good dumps and stores a "verified" flag in the index. No ROM hashing on
    the console.
  - Compatibility warnings before launch: warn if a game needs the Expansion
    Pak and none is detected; badges for Controller Pak, Rumble Pak, Transfer
    Pak (data from the metadata index).
  - Remember the selected game per folder.
  - Automated screenshots in the ares emulator for the README and for
    catching visual regressions.
  - Memory safeguard on the console: when a theme or setting asks for more
    memory than is free, fall back quietly instead of failing (for example
    draw the background live instead of loading a PNG, or leave side covers
    as placeholders, which already happens), and say why in a short note in
    Settings. The Theme Maker's RAM meter (v0.4) only estimates; this is what
    guarantees the menu never crashes.
- **v0.3 organizing:** continue row, launch stats, smart collections, region
  dedupe, letter-wheel search, random game, homebrew/64DD shelves, party mode,
  clock.
  - Missing art report: a screen listing games without box art, screenshots
    or descriptions.
  - Screenshot button: a button combo saves the current screen to the SD card
    as an image (use safe file writes).
  - Custom collections (user-made lists, alongside smart collections).
  - Recently added collection (newest files first).
  - Hide individual entries without deleting them.
  - Emulator shelves for NES, SNES, GB, GBC with matching cover shapes.
  - 64DD pairing: link a disk to its cartridge so both launch together.
  - Game variants: base game plus hacks/translations under one cover, with a
    picker for which version to boot.
- **v0.4 themes:** theme picker, codes + QR on console, box art tinting, blurred
  art background, music, seasonal themes, profiles, settings backup.
  - Font and sound packs: theme.ini can point to a custom font and UI sounds.
    Build on the existing custom font support in `fonts.c`.
  - Theme Maker: suggest a palette from a box art image or photo.
  - Theme author guide: a doc explaining every theme.ini key, with CRT tips
    (contrast, avoid 1px lines, overscan safe areas).
  - Separate volume controls for UI sounds and music.
  - Theme Maker: contrast auto-fix button and randomize button.
  - Theme Maker RAM meter: a bar showing the theme's estimated memory use
    against the console's total, with the 256 KB reserve marked, turning
    yellow and then red as it gets close. A console selector (4MB, 8MB, later
    Analogue 3D) so nobody has to do the sums. Tapping the bar lists what
    each piece costs ("PNG background: 600 KB", "Cover cache, 5 covers:
    180 KB") so people can see what to switch off. The costs come from a
    small table generated from hardware logs for each menu release, not from
    guesses. Worth building once themes can change memory use by much (PNG
    backgrounds); today only the side covers and cover art do.
  - Installable web app: turn the Theme Maker into an installable page. The
    companion app comes after this.
  - Community theme repo on GitHub: one folder per theme, issue-form
    submissions (paste a share code), an Action that validates, renders
    previews, and rebuilds index.json. License themes CC0/CC-BY; no
    copyrighted images.
- **v0.5 polish:** setup wizard, accessibility, overscan + CRT test patterns,
  240p mode, wraparound scrolling, rumble, attract mode, what's new screen,
  README/FAQ, theme gallery, acknowledgements + AI disclosure.
  - Setup wizard and the intro: the wizard asks whether the player wants
    the intro and the fade-in, and on that first run plays the intro and
    then fades in (today the fade only draws on the Games screen, so the
    first run, which opens on the credits, fades the music only).
  - Controller test screen: button presses and stick range. Put it in the
    same settings area as screen calibration (overscan + CRT test patterns).
  - Widescreen layout: optional 16:9 anamorphic layout for stretched TVs.
  - System info screen with a QR code (version, RAM, region, video mode,
    theme) for bug reports.
  - Beta channel: test builds published separately from stable releases.
  - Autoboot: boot straight into the last game; hold a button for the menu.
- **Later:** on-console theme editor, video previews (screenshot slideshow
  fallback), save-file achievements, PNG backgrounds, cover grid layout.
  - Manual viewer: browse pre-converted manual page images one at a time.
    Must fit in 4MB (load one page at a time).
- **After that: Analogue 3D support.** A new phase once v0.5 and the "Later"
  items are done. Do not start it before the native N64 menu is finished.
  - Detect when running on an Analogue 3D.
  - When detected, use 32-bit color for backgrounds and covers; keep the
    16-bit dithered path everywhere else.
  - Make sure the menu draws full frames so it looks right with the A3D's
    "Force Progressive Output" setting on and off.
  - The A3D's built-in Expansion Pak is already handled by the Expansion Pak
    check; just verify it.
  - Add Analogue 3D to the release test checklist.
- **Companion app** (separate repo, after the installable web app): SD card
  setup wizard, update checker, asset prep tool merged in, save manager,
  settings editor, theme gallery browser (QR scanning on Android), collection
  overview.

## Decide early
- 240p vs 480i (affects every layout).
- Profile-aware storage paths for stats/favorites/index from the start.
- Keep all new user-facing strings in one file.
