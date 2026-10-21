# SummerStation64 handoff

Written 2026-10-21. The short version of where things stand. Conventions,
file layout, every measurement and the reasons behind each choice are in
`CLAUDE.md`; read this first, then that.

## Goal

A themeable game front-end for the SummerCart64 on a real N64, forked from
N64FlashcartMenu on libdragon. It must run on a stock 4MB console. The owner
(Kerillian) tests every build on hardware (composite CRT; Expansion Pak and a
Jumper Pak) and is learning as the project goes, so explain changes plainly.

## How a step goes

1. One change, in a new file where possible, with a small hook in the stock
   file. `make sc64` in the dev container (warnings are errors).
2. Stop. The owner deploys from Windows: `.\deploy-sd.bat /d` (onto the card,
   with the log), or `.\tools\sc64\sc64deployer.exe upload <file>` then
   `... debug --no-writeback` (runs a file from the cart's memory, card
   untouched; unplug USB to go back). **Never `/dur`.**
3. "All testing passed" means commit on `summerstation`. Push to `fork` only
   when asked; never to `origin`.
4. PC tests: `sh tests/run.sh` (theme.ini reader, hack finder, share codes).

## Current state

- **v0.1 is published** on GitHub (2026-10-20) and posted on Reddit; people
  like it, no complaints so far. Two files on the release: the Art Edition
  `sc64menu.n64` (default, covers and game details built in) and
  `SummerStation64-v0.1-Standard.zip` (details built in, covers from the
  card). Text: `docs/RELEASE_NOTES_v0.1.md`. The files are kept in
  `output/release-v0.1/` (never `make clean`, it wipes them).
- **The repository's front page** is `README.md` on the default branch
  `summerstation` (renamed from `carousel-ui`). The owner edits it on GitHub
  too: always fetch before pushing.
- **v0.2 has started.** First step done 2026-10-21: upstream
  N64FlashcartMenu v0.3.5 merged, tested and pushed (`767dfb12`).
- **Kept on this machine only:** branch `before-upstream-0.3.5` and the old
  libdragon install `/opt/libdragon.before-0.3.5`, in case the merge's
  slower background build ever matters.
- Never commit the owner's art or details (`assets/boxart/source/`,
  `filesystem/art/`, `filesystem/meta/`); they go only into release files.

## What changed since the last handoff (2026-10-14)

All tested on hardware and committed.

| Change | Main files |
|---|---|
| Roadmap sorted by version, each item done / partly / not started | `CLAUDE.md` (Release plan) |
| Controller, Rumble, Transfer Pak badges; Settings > Display > Accessory Badges (Off / Plain / Colored, blue) | `game_index.c`, `game_facts.c`, `games_ui.c`, option `pak_badges` |
| Settings > System > Memory: free memory and what was left out for lack of it | `memory_note.c` (new), hooks in `carousel_art.c`, `baked_art.c`, `intro.c` |
| 4MB test passed on the Jumper Pak; real memory figures recorded | `CLAUDE.md` (Memory budget) |
| Game details (descriptions, dates, developers, players) baked into the menu file | `baked_meta.c` (new), `assets/boxart/make_baked_meta.py` (new), hooks in `game_facts.c`, `game_learn.c`, `rom_info.c` |
| Hacks worked out again once the idle reader learns new codes | `games_filter.c`, `game_learn.c` |
| Screenshots: START on controller 2 saves `menu/screenshots/shotNNNN.png` | `screenshot.c` (new), hooks in `actions.c`, `menu.c` |
| Release: two editions, formatted notes with AI disclosure and a feedback box; README front page; upstream's README kept in `docs/` | `docs/RELEASE_NOTES_v0.1.md`, `README.md`, `docs/UPSTREAM_README.md` |
| Upstream v0.3.5: autoload fixes, low-voltage error, newer libdragon and libraries, the menu's own metadata | `views/startup.c`, `metadata/` (now describes SummerStation64), `game_facts.c`, `game_info_ui.c` (Expansion Pak enum renamed) |

## What failed or did not work

- **The background build got slower with the merge:** 470 ms against 300 ms
  for the v0.1 file, same console and settings. Not explained (same
  compiler and flags; probably code layout against the 8 KB data cache, as
  Ocean's earlier creep). Not noticeable; parked for the pre-v1.0 speed run.
- **The Folders tab doubles every frame with the Full character set** (fine
  on Auto). First blamed on the merge; a comparison showed it was the font.
- **Image viewer, 4MB, once:** "PNG decode failed due to insufficient
  memory" after quick cover browsing. Handled cleanly, not reproduced; fix
  idea in `CLAUDE.md` if it returns.
- **No one-controller screenshot button was possible:** L, R and Z act the
  moment they are pressed, and the stock code reads every port.
- **A separate art repository** was considered for the Art Edition and
  dropped; the art lives only inside the release file.
- **The owner's editor saved an old copy of the release notes over a newer
  one** (restored from git). When a file changes on disk, `git diff` it
  before building on it.
- **Pak badge colours:** red, blue and yellow were tried; the owner chose
  blue for all three.

## What the next session should do

The owner's plan: **finish v0.2, then build the release tooling and release
v0.2.** Order:

1. **Answered 2026-10-21:** perspective covers stay part of the optional
   side covers (off by default); the GitHub bug-report form ships with v0.2.
2. ✓ **Live background** (done 2026-10-21: 300 KB freed, chip 3 ms faster).
3. **Perspective covers**, on the optional side covers.
4. **PC prep tool:** one program that prepares a card (bakes art and
   details, builds `gameindex.txt` ahead, checks dumps).
5. **"Verified dump" badge and genre**, once there is a source (check the
   terms of No-Intro's lists first).
6. Proposed: move "automated screenshots in ares" to Later.
7. Then: the bug-report form, `make release` (both editions in one command; the manual steps are
   in `CLAUDE.md`, Release plan, v0.1) and the v0.2 release.

Waiting on the owner, later: what a frame looks like on the redesigned
screens (Frame Borders, v0.4), a list of stand-alone hacks (v0.3 hacks step
2), Analogue 3D testers (asked in the README).
