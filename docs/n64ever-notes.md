# N64ever: what it has and how it maps to our roadmap

Notes from reading the N64ever fork, to decide what is worth learning from or
adapting for SummerStation64. Nothing from N64ever has been merged or copied
into this repository.

- Source: https://github.com/bjerreman/N64FlashcartMenu-N64ever
- Read at: branch `n64ever`, commit `0bc801d2` (tag `release-0.3.2-n64ever-v1`)
- Local remote: `n64ever`, fetch only (its push address is disabled)
- Written: 2026-10-07

## How this was read

Read in full: the README, `NOTICE`, the changes to `menu.c` and `actions.c`,
and the headers of the new data modules (`disclink.h`, `game_special.h`,
`game_metadata.h`, `rom_custom.h`, `rom_boot.c`).

Read in part: `games_grid.c` (3,889 lines; the cover-loading and memory
section, and a search for how it draws and times things).

Not read line by line: the 2,300-line change to `browser.c`, and the changes
to `boxart.c`, `common.c`, `load_rom.c`, `settings_editor.c`,
`history_favorites.c` and `bookkeeping.c`. For those, the notes below rest on
the README's description and on searches of the source, and say so where it
matters. Effort estimates are judgements, not measurements. Nothing from
N64ever was built or run.

## What N64ever is

A personal fork of N64FlashcartMenu built around a **Favorites Grid**: a
masonry wall of cover art as the home screen, with everything else as pop-ups
over it. It bakes about 767 games' box art and a 445-game text database into
the ROM, so it needs no art on the SD card.

It arrives as a single commit on top of upstream, so there is no history to
follow feature by feature.

## Ground rules for using it

- **Licence:** GNU AGPL v3, the same as upstream and as this project. Code can
  be reused under those terms.
- **Notices:** its `NOTICE` file states that no separate copyright is claimed
  over the modifications. Even so, any file we take or adapt keeps whatever
  header it has, and gets a line saying it came from N64ever.
- **Credit:** N64ever (Bjerreman and contributors) goes in our
  acknowledgements when the first piece of its code lands.
- **Do not copy** the baked-in box art (`assets/images/boxart/`, 3,160 PNG
  files, 105 MB) or the game descriptions in `src/menu/game_metadata_db.h`
  and `src/menu/game_special.c`. The descriptions are original writing; the
  art is scans of commercial packaging. The *structure* of the database and
  the lookup code are fair to learn from; the contents are not ours to take.
- **Do not merge the branch.** It rewrites most of the files we have also
  changed, in a different direction.

## Where our base differs

N64ever is based on upstream **V0.3.2** (`6407ab15`). Our fork is based on
upstream `e28c26e1`, which is V0.3.4 plus three commits. So our base has five
upstream commits N64ever lacks:

- Release V0.3.3 and Release V0.3.4
- "Update boot.c", "Update Dockerfile.docbuilder", "Fix Aleck64 conversions"

Those releases changed 25 of the same source files N64ever edits, by about
1,300 added lines. The parts that matter most for us:

- **4MB protection.** Upstream added the Jumper Pak limits (1,024 entries per
  folder, 512 per archive) and the low-memory handling in the game info screen
  and image viewer. N64ever's `browser.c` has none of the Jumper Pak limits.
- **`ini_parser.c`** was largely rewritten upstream (368 lines changed).
  N64ever's own changes to it are against the old version.
- **`png_decoder.c`**, **`rom_info.c`**, **`sound.c`**, **`background.c`**,
  **`load_rom.c`** all moved upstream as well.

In practice: N64ever's *new* files (`disclink.c`, `game_special.c`,
`rom_boot.c`, `link_disc.c`, `game_metadata.c`) are the easy ones to adapt.
Its edits to *existing* files would have to be redone by hand against our
versions, not applied.

## N64ever and memory

N64ever does not target 4MB. The only place it looks at memory size is the
cover cache: 80 covers with the Expansion Pak, 8 without. Its favorites list
is capped at 2,048 entries, each with a cached record that its own README
calls "a sizable per-game struct". It ships two full fonts, a splash sprite,
placeholder sprites and extra sound effects, all held in RAM.

Anything taken from it has to be re-budgeted against our memory table, and
anything that scales with the number of games needs a cap for 4MB.

## Feature map

Effort: **small** = a day-sized change that fits our "new file plus a hook"
pattern; **medium** = several steps, each testable; **large** = a rewrite of
the idea in our own structure.

### Already built here, in a different form

| N64ever feature | Ours | Notes |
|---|---|---|
| Cover-art home screen (masonry grid) | Carousel (`browser.c`, `carousel_art.c`) | Different layout. Their grid is the "cover grid layout" item under **Later**. |
| Inspect pop-up (cover, name, developer, date, region, description) | Game info screen (`game_info_ui.c`) | Same job. Theirs scrolls long descriptions with C up/down, which ours does not. |
| Lazy cover loading, one decode at a time, nearest first | `carousel_art.c` (five-slot cache) | Same approach. See "Techniques worth borrowing" for their memory check. |
| 60 fps cap | `frame_rate.c` (30 / 60 setting) | See "60 frames a second" below. |
| Boot splash, custom splash PNG | Not built; **v0.2** "boot animation" and redesign stage 7 "intro" | Theirs is a still image, not an animation with music. |
| Settings as an always-open list | `settings_menu.c` | Ours has categories and descriptions. |
| A-Z jump with C left/right | Position bar shows the letter; jumping is **v0.3** "letter-wheel search" | Theirs is a simple jump between letter groups, which is a cheap first version of ours. |
| Region fallback for art (PAL game uses US cover) | Stock loader already drops the region letter | Theirs tries a list of other regions, for baked art only. |

### Maps to a roadmap item we have not started

| N64ever feature | Roadmap item | Their files | Effort to adapt | Memory and 4MB |
|---|---|---|---|---|
| **64DD disc linking**: an expansion disc is linked to its base cartridge by game code, in two hand-editable files (one for Japanese discs, one for the rest); launching an unlinked disc offers to pick the base ROM | **v0.3** "64DD pairing" | `disclink.c/.h` (104 + 37 lines), `views/link_disc.c` (418), changes in `load_disk.c` | Small to medium. `disclink.c` is self-contained and close to usable as is; paths need moving from `menu/n64ever/` to our generic `menu/`, and writes need our temp-file-then-rename helper. The picker screen would be rebuilt in our style. | Negligible. Small text files read on demand. |
| **Special editions**: ROMs that reuse another game's code (Master Quest, Smash Remix) are told apart by words in the file name, and get their own name and art key | **v0.3** "Game variants" | `game_special.c/.h` (124 + 63) | Small for the matching idea. The table's entries include descriptions we must not copy; we would write our own entries, or better, read them from a file on the card so nothing is baked in. | Negligible. |
| **Build-type labels**: Demo / Prototype / Beta recognised from tags in the file name; Aleck64 and iQue flagged as "may not boot" | **v0.2** "Compatibility warnings before launch" (extends it) | `game_special.c` (`game_platform_classify`, the build-type function) | Small. Pure string matching, no data. Fits as extra badges in `game_facts.c`. | None. |
| **ROM boot**: a chosen game starts at power-on after a countdown showing its art; B cancels, holding Start at boot disables it | **v0.5** "Autoboot: boot straight into the last game; hold a button for the menu" | `views/rom_boot.c` (166), hooks in `startup.c`, `settings.c` | Small. It is a self-contained screen. Ours would boot the *last* game, which the History list already knows. Note upstream also has a separate autoload option; the two should not both exist. | One cover while the countdown shows. |
| **Idle screensaver**: scrolling rows of covers after a timeout or holding L+R | **v0.5** "attract mode" | Inside `games_grid.c` (search `ss_`) | Medium to large. It is woven into the grid file and into its cover cache, and it frees the grid's covers to make room. The idea transfers; the code does not lift out. | Their own limit is five rows "a RAM limit". On 4MB this needs a hard, small cap, or to be Expansion Pak only. |
| **Built-in text database**: name, developer, three regional release dates, description, binary-searched by the first three letters of the game code | **v0.2** "metadata index" with "year, developer, publisher, genre" | `game_metadata.c/.h` (109 + 80), `game_metadata_db.h` (1,368, **do not copy**) | Medium. The lookup, the three-letter key shared across regions, and the per-game override file are a sound design to follow. Our plan is an index built on the PC by the asset prep tool, which avoids baking text into the ROM at all. | Their table is compiled into the program, so it costs RAM on every console whether used or not. An index file read from the card costs nothing until opened. Prefer ours. |
| **Game Metadata register**: per game, which of six art types and which text sources exist | **v0.3** "Missing art report" | Inside `games_grid.c` | Medium. Theirs reports on one game; ours is meant to list all games with gaps. Useful as a model for what to check. | Low. |
| **Per-game art and text overrides** in a folder on the card | **v0.3** "Game variants", and **v0.4** theme items | `rom_custom.c/.h` (158 + 32), parts of `boxart.c` | Small to medium. | Low. |
| **File management**: copy, move, rename, create folder, with an on-screen keyboard; cancellable copy with a progress bar | Not on the roadmap | Inside `browser.c` | Large. It lives in the file we have changed most, against an older base. The on-screen keyboard would be worth having on its own (search, renaming collections). | Low. A copy needs a buffer. |
| **Favorite a whole folder**, hold B to favorite, 2,048 favorites, favorites and history in separate files | **v0.3** "Custom collections" | `bookkeeping.c/.h` (288 + 71 lines changed) | Medium. The split into two files and the migration from the combined file are sensible. The 2,048 cap is not safe for us. | The cap must be far lower on 4MB, or the list must not be held in RAM whole. |
| **History of 64** (stock is 8) | **v0.3** "continue row", "launch stats"; improves our "Recently Played" sort | `bookkeeping.h` | Small. | 64 paths is a few KB. Fine. |
| **PAL60 with a 10-second confirm** that reverts if the picture breaks | Not on the roadmap; fixes a real stock hazard our Settings screen still has | `settings_editor.c` | Small. Our PAL60 row currently warns that a wrong choice needs the SD card edited by hand. This removes that. | None. |
| **Boot timing readout**: milliseconds spent in each start-up phase, logged and shown on screen | **v0.1** "debug overlay" | `menu.c` | Small. Complements our `stats:` line. | None. |
| **Sound effects**: small generated effects, a launch sound, user overrides from the card | **v0.4** "Font and sound packs", "music" | `tools/gen_sfx.py` (150), `sound.c`, `assets/sounds/` | Small to medium. The generator script is the interesting part: their nine effects total about 86 KB of WAV where stock's five total about 440 KB. | Smaller effects mean less in the ROM. RAM use depends on how they are played. |
| **Blank-cartridge placeholder** when a game has no art | Fits **v0.3** "Missing art" and general polish | `assets/images/placeholder-*.png` (**their artwork, do not copy**), `boxart.c` | Small for the idea; we would need our own picture. | One sprite held in RAM. |
| **Pixel font as default** (PixelMplus 12, monochrome) | **v0.4** "Font and sound packs" | `Makefile`, `fonts.c`, `assets/fonts/` | Small. A monochrome font is far smaller in RAM than our outlined one. Whether it reads well on composite is a matter for the CRT. | Could cut font memory further than our Latin-only font did. Worth measuring. |
| **Wrap-around movement** on the grid | **v0.5** "wraparound scrolling" | `games_grid.c` | Small in our carousel (the stock wrap setting already exists). | None. |

### Not a fit, or deliberately not taken

| N64ever feature | Why not |
|---|---|
| Baked-in box art (767 games) | Not ours to copy, and it would make the ROM far larger. Our art stays on the card. |
| Favorites as the only home screen | We browse folders as well as favorites. |
| One-time wipe of favorites and history on upgrade | We should never delete a user's data on update. |
| `menu/n64ever/` as the data folder | Our rule is generic folder names under `menu/`. |
| Removal of the custom background picture | We keep it. |

## 60 frames a second

N64ever sets the cap to 60, and its README says "60 fps render". Two things
in its source qualify that:

- Its own comments say "the grid framerate varies", and it switched its
  screensaver timers from frame counts to the clock for that reason.
- Its backgrounds are solid colour, drawn with the graphics chip's fill mode,
  which is the cheapest thing it can draw. It has no gradient or pattern
  background and no blurred-cover backdrop.

So N64ever does not contain a technique that makes our screens hit 60. What
it shows is that 60 is reachable when the background is nearly free. Our own
measurements (2026-10-06, before the two speed-ups still waiting for a test)
were 16 to 18 ms of work per frame on Settings and about 24 ms on Game info,
against a budget of 16.7 ms. The lesson for us is the same one those numbers
point to: the full-screen background is where the time goes.

Two things it does that we should copy if we ship 60:

- **Input repeat tuned for 60** (`actions.c`): a longer first delay (10 frames)
  and then accelerating repeats (every 5, then 3, then every frame). Stock
  uses a fixed 8-frame delay then every frame, which at 60 is twice as fast
  as it was designed to be. This is a small, self-contained change.
- Most of its animations are stepped **per frame**, with speeds halved "for
  the 60fps render". That is the opposite of our rule that animations are
  timed by the clock, so its animation code should not be taken as is.

## Techniques worth borrowing

- **Check for a real block of free memory, not the total.** `art_can_fit()` in
  `games_grid.c` tries to allocate one block the size of the largest cover
  and frees it straight away. Their comment explains why: after covers of
  different sizes have come and gone, an 80 KB allocation can fail with
  megabytes free in small pieces, and libdragon's asset loader stops the
  program on a failed allocation instead of returning an error. Our side-cover
  check (`SIDE_ART_MIN_FREE` in `carousel_art.c`) compares total free memory,
  so it has exactly the blind spot they describe. This is the single most
  useful thing in the fork for us, and it is a few lines.
- **Never free a cover that is still decoding.** They skip those when evicting,
  noting that freeing one aborts the decode and can leave the decoder's
  callback pointing at freed memory. Worth checking our cache against.
- **Load text before art, and never let art block text.** Their loader reads
  a game's facts first and only then queues its cover.
- **Draw something before slow start-up work.** They show the splash, then load
  favorites and history behind it, instead of a black screen.
- **A marker file for one-time migrations** (`.migrated.vN`). Sound as a
  mechanism, even though we would not use it to wipe data.
- **Their build traps** (README section 2): a stale ROM left in `output/` when
  a build step fails quietly, and a stale asset pack when files are copied in
  with old dates. Both could bite us.

## Suggested order, if we take anything

1. The free-block check for covers. Small, and it closes a real gap.
2. Input repeat tuned for 60, if the frame rate work is kept.
3. PAL60 confirm-with-timeout. Small, removes a way to lock yourself out.
4. Build-type and hardware-variant badges. Small, no data needed.
5. 64DD disc linking, when v0.3 is reached. Their `disclink.c` is the best
   candidate for near-direct reuse, with credit.
6. Autoboot, when v0.5 is reached.

Everything else is better treated as a design reference than as code to
bring across.
