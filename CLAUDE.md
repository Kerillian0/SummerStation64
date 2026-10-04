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
  `carousel_draw()`), uses theme colors; up/down rotates; left/right still
  switch the History/Favorites tabs.
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
- Done and tested on hardware: step 1 (theme loader), step 2 (text colors).
- **Next: step 3a bundle**
  1. menu_features + theme `[features]` + `theme.txt` support (code written,
     not yet built). Browser counter shows `4MB`/`8MB` temporarily.
  2. See-through side covers and hollow selection ring in `browser.c`
     (code written, not yet built).
  3. Theme the original frame, tabs and bottom bar: read
     `src/menu/ui_components/constants.h`, map each frame color to new
     `[colors]` keys in theme.ini, so the community can edit them. The web
     Theme Maker needs matching fields and a "current layout" preview, and the
     share code format must bump to v2 (v1 codes must keep working).
- Then step 3b: user toggles in the Settings screen.

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
