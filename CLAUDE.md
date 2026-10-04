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

## Build and test
- Build inside this dev container: `make sc64`
- Output: `output/sc64menu.n64` — the user copies it to the SD card root.
  (The default `make` target only builds the generic ROM.)
- Compiler treats warnings as errors. On this toolchain `int32_t` is `long`:
  cast to `(int)` for `%d` in printf-style calls.
- New .c files must be added to the source list in the Makefile next to
  `menu/sound.c \` (same format, trailing backslash).

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
  `carousel_draw()`), uses theme colors.
- `src/menu/carousel_art.c/.h` — box art on the center cover. Reads the game
  code from the ROM header itself, reuses the stock boxart loader, keeps one
  image in memory, loads 250 ms after the selection settles, and is freed
  when leaving the browser (the PNG decoder handles one image at a time).
  Feature `cover_art` (default on).
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
- `src/menu/controls.c/.h` — button layout for the three tabbed screens:
  L/R switch tabs, Z is Options (was R), left/right scroll the carousel,
  up/down do nothing there unless the `updown_scroll` feature is on. It
  rewrites the action flags so the stock screen code is untouched (one-line
  hooks in `browser.c` and `history_favorites.c`). Also tells a tap of A from
  a hold (500 ms) for `quick_launch` (tap starts, hold = info) and
  `hold_launch` (tap = info, hold starts); `load_rom.c` has a one-line hook
  that starts the game when asked. Browser Options has a "Game info" entry.
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
- Still open from part 3: the web Theme Maker (not in this repo) needs matching
  fields and a "current layout" preview, and the share code format must bump to
  v2 (v1 codes must keep working).
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
- **v0.1 feature list is complete.** Before publishing, the user still wants
  to test Japanese (tall) and 64DD-shaped cover art. Next: v0.2.

## theme.ini format (v1)
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
quick_launch = 1       ; any key from menu_features.c
```

## Share codes (v1, used by the web Theme Maker)
32 bytes, Crockford base32, shown as `SS64-XXXX-...`.
[0] version=1; [1..24] colors RGB: text, text_dim, accent, panel, color1,
color2, color3, pattern color; [25] bits0-1 bg type, bits2-3 direction,
bit4 color3 on, bit5 dither; [26] pattern; [27] size; [28] opacity;
[29] feature toggles (reserved); [30..31] CRC-16/CCITT-FALSE of bytes 0..29.

## Release plan
- **v0.1 usable carousel:** theme loader ✓, text colors ✓, feature toggles +
  Expansion Pak check, safe mode + friendly crash screen, left/right browsing,
  quick launch (skip the ROM info screen), box art on covers.
- **v0.2 smooth and safe:** metadata index, cover cache, perspective covers +
  slide animation, SteamOS-style boot animation, save backups, sort options,
  4MB-friendly live background, asset prep tool.
- **v0.3 organizing:** continue row, launch stats, smart collections, region
  dedupe, letter-wheel search, random game, homebrew/64DD shelves, party mode,
  clock.
- **v0.4 themes:** theme picker, codes + QR on console, box art tinting, blurred
  art background, music, seasonal themes, profiles, settings backup.
- **v0.5 polish:** setup wizard, accessibility, overscan + CRT test patterns,
  240p mode, wraparound scrolling, rumble, attract mode, what's new screen,
  README/FAQ, theme gallery, acknowledgements + AI disclosure.
- **Later:** on-console theme editor, video previews (screenshot slideshow
  fallback), save-file achievements, PNG backgrounds, cover grid layout.

## Decide early
- 240p vs 480i (affects every layout).
- Profile-aware storage paths for stats/favorites/index from the start.
- Keep all new user-facing strings in one file.
