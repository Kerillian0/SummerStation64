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
  - `.\localdeploy.bat /dur` — same, then asks the running menu to save
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
    restarts. A single `dropped due to timeout` line can still appear and was
    harmless.
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
  line to a category. Changes are held in memory and written once, when
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
  once (RGBA16 with 4x4 Bayer dither) on first draw.
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
                       ; see_through_covers, remember_selection
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
bytes 0..49. The Theme Maker writes v1 when the frame colors are stock and no
feature is set, so simple themes keep the short code. Both versions decode.

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
  (`DITHER_BAYER_NONE`). Tried first as 16-bit with baked dither (150 KB): the
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

Still to measure: a real 4MB run, a folder with many entries, the image
viewer and the music player.

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
  - Installable web app: turn the Theme Maker into an installable page. The
    companion app comes after this.
  - Community theme repo on GitHub: one folder per theme, issue-form
    submissions (paste a share code), an Action that validates, renders
    previews, and rebuilds index.json. License themes CC0/CC-BY; no
    copyrighted images.
- **v0.5 polish:** setup wizard, accessibility, overscan + CRT test patterns,
  240p mode, wraparound scrolling, rumble, attract mode, what's new screen,
  README/FAQ, theme gallery, acknowledgements + AI disclosure.
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
