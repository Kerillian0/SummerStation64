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

## Where things are
```
CLAUDE.md                  this file: the full project record
Makefile                   source list (add new .c files beside menu/sound.c)
                           and the font rules (Latin, Title, Title20)
localdeploy.bat            stock: run a build from the cart's memory (no /dur)
deploy-sd.bat              ours: put a build on the SD card
assets/fonts/              Firple-Bold.ttf and the charset-*.txt lists the
                           fonts are built from
docs/HANDOFF.md            current state and next steps
docs/n64ever-notes.md      what the N64ever fork has, mapped to our roadmap
docs/*.md (numbered)       upstream's user documentation, untouched
theme-maker/               web Theme Maker source (one HTML file) + README
tools/sc64/                sc64deployer.exe, not committed (tools/ is ignored)
output/sc64menu.n64        the build
libdragon/                 the SDK, a submodule; do not edit
src/menu/                  menu code; our new files sit beside the stock ones
src/menu/views/            one file per screen
src/menu/ui_components/    stock shared drawing code (small hooks only)
```
Ours (new files, free to change), all in `src/menu/`: `theme`,
`menu_features`, `menu_options`, `safe_file`, `safe_mode`, `crash_screen`,
`debug_stats`, `controls`, `carousel_art`, `folder_memory`, `sort_order`,
`display_name`, `font_choice`, `title_font`, `frame_rate`, `games_ui`,
`game_facts`, `game_info_ui`, `play_stats`, `intro`, `menu_name.h`, and
`views/settings_menu`
(`views/features_menu` is ours too but no longer reachable).

Stock files we have edited (keep these edits small): `menu.c`, `actions.c`,
`fonts.c/.h`, `rom_info.c/.h`, `ui_components/background.c`, `common.c`,
`constants.h`, `views/browser.c` (the carousel lives here, the one large
edit), `views/history_favorites.c`, `views/load_rom.c`, `views/load_disk.c`,
`views/startup.c`, `views/settings_editor.c`. `git diff --stat origin/main..carousel-ui` lists
them all.

Files the menu keeps on the SD card, all under `sd:/menu/`:
| File | Holds | Written by |
|---|---|---|
| `theme/theme.ini` (or `theme.txt`) | the theme; read only | the user / Theme Maker |
| `features.ini` | the player's On/Off choices | `menu_features.c` |
| `options.ini` | `sort_order`, `font`, `frame_rate_experiment`, `intro`, `fade` | `menu_options.c` |
| `folders.ini` | selected entry per folder | `folder_memory.c` |
| `playstats.txt` | play count and last played | `play_stats.c` |
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
  `tab_inactive`, thumb in `accent`). Since the font row moved to System no
  page is long enough to scroll, so the bar is currently never shown. The screen reopens where it was left
  (feature `remember_settings`, default on, this power-on only). Categories:
  Display, Controls, Sound, Library, Files, System. Changes are held in memory and written once, when
  leaving the screen or after 5 s without a change (the user asked for fewer
  SD card writes); `features_user_change()`/`_flush()` and
  `options_change()`/`_flush()` exist for that.
- `src/menu/debug_stats.c/.h` — prints a `stats:` line to the debug log every
  two seconds (heap size, used, free, average and worst frame time), restarted
  on each screen change. One-line hook in the `menu.c` main loop. Screen
  numbers are `menu_mode_t` values: 2 Files, 9 Settings, 15 game info,
  21 Favorites, 22 History. Read it with `localdeploy.bat /dur`.
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
- `src/menu/menu_options.c/.h` — the player's non-switch preferences (so far
  only `sort_order`), saved to `sd:/menu/options.ini` (temp file + rename).
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
  (see "Menu redesign"): tab bar with L/R badges and a clock, title panel,
  position bar. Colors come from existing theme keys (`tab_active`,
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
- **v0.1 user-facing features are complete.** Two dev-tooling items were
  added to v0.1 afterwards and are not started: the debug overlay and the
  PC-side tests. Before publishing, the user still wants to test Japanese
  (tall) and 64DD-shaped cover art.

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
type = gradient        ; solid | gradient | image
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
                       ; hide_extensions, tidy_titles, hide_tags
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
Constraints found so far: rounded corners are not cheap on the N64 (square
corners used); small label text from the mockups would not be readable on a
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
