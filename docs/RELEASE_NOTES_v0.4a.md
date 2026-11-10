# SummerStation64 v0.4a

<p align="center">
  <b>☀️ A themeable game menu for the SummerCart64, built on a real N64 and a CRT. ☀️</b><br>
  <img src="https://img.shields.io/badge/SummerCart64-menu-f2b134" alt="SummerCart64 menu">
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

**v0.4a** is the first part of v0.4, the themes release. **Make a theme
right on the console** with a color grid and sliders, keep **up to 32
themes** on your SD card, each with its own **music, button sounds, intro
tune and font**, and set **music and sound volume** separately. Up to
**four players** can each have their own saves, favorites, collections,
theme and **profile picture**. Also new from your requests: **mark any game as homebrew or a ROM
hack**, **64DD disks show their covers** on the cover rows, the **intro's
logo and title can each be switched off**, and names are typed on an
**on-screen keyboard**. Thank you to everyone who wrote in!

> [!NOTE]
> **🌟 Our first outside contributor: [stewx](https://github.com/stewx)!**
> This release brings their **title logos** and **video previews** on Game
> info (the game's own logo in place of its name, and a short clip of the
> game playing in the cover's place) and **review scores**. Both came as carefully made and tested
> pull requests, with the tools to make the logos and clips for every game.
> Thank you, stewx!

New here? Everything v0.3 brought (3D boxes and cartridges, collections,
cheats, the Retro and Homebrew tabs, party mode, clips) is in the
**[v0.3c release notes](RELEASE_NOTES_v0.3c.md)**.

> [!TIP]
> **Quick start:** download **`sc64menu.n64`** from the Assets below, copy it
> to the top folder of your SD card, switch on. Covers and game details are
> built in.

> [!NOTE]
> **Coming from v0.3c?** Copy the new `sc64menu.n64` over the old one; your
> settings, favorites, collections and save backups stay, and become the first
> profile's. A few defaults changed, for settings you never chose yourself:
> **Previous/Next Covers** are on, the **Intro Logo** is Classic, the
> **Selection Marker** is off, and names are typed on the new **keyboard**
> (Settings > Controls > Name Entry brings the dial back). **Frame Borders**
> is gone.

---

## ✨ New in v0.4a

### 🎨 Themes
- **Make a theme on the console:** **Settings > Display > Edit Theme** starts
  from the theme you're using. Change the text, accent, panel, tab bar and
  other colors, the background (solid, gradient or ocean, with a middle color
  and a direction) and the pattern; the whole menu shows each change as you
  make it, with a small sample of the Games screen beside the list.
  - **Picking a color:** a grid of ready colors, with **Hue / Color / Light**
    sliders underneath for any shade in between. B puts the old color back.
  - **Save as Theme...** names it and saves it in `menu/themes/<name>/`, then
    switches to it. The file is the same kind the Theme Maker writes. Saving a
    theme that has its own music, sounds or font under a new name asks
    whether to bring them along, and shows how much that is.
  - **New Theme** starts from a look you pick (any built-in theme, or plain
    dark) instead of the one you're using.
- **Themes in folders:** `menu/themes/<name>/theme.ini`, up to 32, listed in
  **Settings > Display > Theme** after the built-in ones. The older
  `menu/theme/theme.ini` is still **From SD Card**.
- **Sounds and fonts in a theme:** a theme's folder can hold its own
  `music.wav64`, `intro.wav64`, button sounds (`move`, `select`, `back`,
  `setting`, `error`) and `font.font64`. The Companion's new
  **[Sounds](https://kerillian0.github.io/SummerStation64/companion/#sounds)**
  tab turns your own audio files into them and zips the theme folder for
  you. Music from the card is read ahead, so it plays without crackling.
- **Theme guide:** every setting a theme can use, what each color colors,
  which pattern sizes cost no memory, and tips for CRTs:
  **[Making a theme](https://kerillian0.github.io/SummerStation64/companion/#guide-themes)**.
- **Round (radial) gradients** no longer slow the menu down.
- **Frame Borders** is gone: it didn't suit the menu's look. A theme that
  sets it still loads, and old share codes still work.

### 👥 Profiles
- **Up to four players** in **Settings > Profiles**, each with a name and a
  color. New Profile, Rename, Profile Color, Delete and Playing As are all there.
- **Each player has their own** saves (and save backups), Recent, Favorites,
  play counts and time played, collections, hidden games, folder places,
  settings and theme. A new profile starts every game with a fresh save.
- **Profile pictures:** **Settings > Profiles > Profile Picture** opens a grid
  of pictures built into the menu, plus any PNG you put in `menu/avatars/` on
  the SD card (64 x 64 is best; the Companion's
  **[Pictures](https://kerillian0.github.io/SummerStation64/companion/#pics)**
  tab makes them from any picture). Each picture sits in a ring of the
  player's color; **Picture Shape** draws them round or as rounded squares.
  A new profile picks one right after it's named.
- **"Who's playing?"** at power-on when there are two or more, with each
  player as a card with their picture, and the last player already chosen.
  The player's picture and name sit under the tabs.
- **Shared by everyone:** the game folders, hack and homebrew marks, 64DD
  pairs, Character Set, Video Output and the menu's own settings (Settings
  says *Same for every player* on those).
- Deleting a profile keeps its saves on the card. The first profile can't be
  deleted: it keeps the saves the menu always had.

### 🔊 Sound
- **Music Volume and Sound Volume** sliders in **Settings > Sound**, 0 to 10,
  5 is the usual level, 10 twice as loud.
- A theme's intro tune plays once, to its end, however long it is.

### 🍺 Homebrew and hacks
- **Mark a game as homebrew** with **Z** (on a cover or on the Folders tab),
  for homebrew with an ordinary game code, such as Junk Runner 64. Z on a
  folder in the Folders tab marks every game in it. *(Thanks, Dispacho!)*
- **Mark a game as a ROM hack** the same way, and **Remove from Homebrew** /
  **Remove from Hacks** undoes either. One rule now decides everywhere, so a
  game is never both.
- With **Hacks / Homebrew on Games: Hide**, the Games tab leaves them out of
  its list and its count straight after a restart, not only after a tab
  switch. *(Reported by Dispacho.)*

### 💾 64DD covers
- **64DD disks show their box art** on the Games, Recent and Favorites cover
  rows, found by the disk's ID as on Game info, with the 3D box's top and
  sides where there are pictures for them. *(Thanks, Quest-Quest!)*

### 🎬 The intro
- **Settings > System > Intro Logo** now has **Off**, and the new **Intro
  Title** row shows the name as a **Picture**, as **Text** or not at all.
  With one of the two off, the other sits in the middle of the screen.
  *(Thanks, Quest-Quest!)*

### ⌨️ Typing names
- **An on-screen keyboard**, laid out like the Switch's, for naming
  collections, profiles and themes and renaming files. A types, B erases,
  R is Shift (twice for caps lock), C-down is a space, C-left / C-right move
  the cursor, START or OK keeps the name. A new name starts with a capital.
- The **dial** is still there: **Settings > Controls > Name Entry**.

### 🎬 Game info: title logos, video previews and review scores *(by stewx)*
- **Title logos:** a game's own logo takes the place of its name, when
  `menu/metadata/<code>/logo.sprite` is on your SD card. A game without one,
  and a hack, shows its name as before.
- **Video previews** *(Expansion Pak)*: after a moment, a short silent clip of
  the game fades in over its cover and loops, from `preview.h264` beside its
  box art. **Settings > Display > Video Previews** turns them off (for the
  whole console). A game without a clip, and a hack, keeps its cover.
- **Getting them:** download **`SummerStation64-v0.4a-Logos-and-Clips.zip`**
  from the Assets and unzip it at the top of your SD card (see Installing).
  They are too big to go inside the menu file. To make your own instead, the
  scripts are in the source (`scripts/fetch_logos.py`,
  `scripts/make_preview_clip.py`, `scripts/fetch_preview_clips.py`).
- **Review scores** *(also by stewx)*: put your own scores in
  `menu/reviewscores.txt` on the SD card and each game shows its score out of
  10 by its name and on Game info (green, yellow or red), with **Review
  Score** in Sort By. No scores come with the menu;
  [the file's format](https://github.com/Kerillian0/SummerStation64/blob/summerstation/docs/review_scores.md).
  Switch them on in **Settings > Display > Review Scores** (off by default).
- A long scrolling description on Game info now draws only the lines that
  show, so Game info has more room to spare.

### 🖼️ Covers
- **Trim Black Edges** now also cleans box **tops and sides**, so the dark
  line at the end of every top label is gone.

### 🧰 The Companion
- **[Sounds](https://kerillian0.github.io/SummerStation64/companion/#sounds)**
  tab (see Themes).
- **[Pictures](https://kerillian0.github.io/SummerStation64/companion/#pics)**
  tab: frame any picture as a profile picture, see it at the console's sizes,
  and download it for `menu/avatars/`.
- New **Guide** sections:
  **[Making a theme](https://kerillian0.github.io/SummerStation64/companion/#guide-themes)**
  (including **[making one on the console](https://kerillian0.github.io/SummerStation64/companion/#guide-themes-console)**),
  **[Around the menu](https://kerillian0.github.io/SummerStation64/companion/#guide-menu)**
  (the intro choices and the keyboard's buttons) and
  **[Profiles](https://kerillian0.github.io/SummerStation64/companion/#guide-profiles)**
  (including making your own pictures).

---

## 📥 Installing

|  | ⭐ **Art Edition** (recommended) | **Standard** |
|---|---|---|
| **File in the Assets** | `sc64menu.n64` | `SummerStation64-v0.4a-Standard.zip` |
| **Size** | about 33 MB | about 2.5 MB |
| **Box covers and cartridges** | built in, appear faster | from the metadata pack on your SD card |
| **Game details** (descriptions, dates, developers, genres, players) | built in | built in |

1. **Update your SummerCart64's firmware** if it is old, and **back up** your
   `sc64menu.n64`.
2. **Art Edition:** copy `sc64menu.n64` from the Assets to the top folder of
   your SD card. **Standard:** unzip `SummerStation64-v0.4a-Standard.zip` and
   copy the `sc64menu.n64` inside; for covers, put the
   [metadata pack](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
   in `sd:/menu/metadata/`.
3. **Optional, for either edition: title logos and preview clips.** Unzip
   `SummerStation64-v0.4a-Logos-and-Clips.zip` at the top of your SD card;
   the files go into `menu/metadata/` beside the box art (nothing is replaced).
   The clips need the Expansion Pak; the logos work on every console.
4. **Switch the N64 on.**

> [!WARNING]
> **"Watchdog timeout" at power-on?** The SummerCart64 gives the menu file 5
> seconds to load. On a card that has been used for a while, the Art Edition
> (about 33 MB) can be scattered across it and take longer. Copy your files
> off, reformat the card (FAT32 or exFAT, for example with the SD
> Association's *SD Card Formatter*), and copy `sc64menu.n64` back first,
> then everything else. If it still happens, use the **Standard** edition:
> it loads as fast as the stock menu.

New to SummerStation64? The Companion's
**[Guide](https://kerillian0.github.io/SummerStation64/companion/#guide-setup)**
walks through setting up a card, including adding your game folders.

<details>
<summary><b>🎨 Sharing a theme</b></summary>

A theme made on the console or in the Theme Maker is a folder:

```
menu/themes/My Theme/theme.ini
                     music.wav64, intro.wav64        (optional)
                     move / select / back / setting / error.wav64   (optional)
                     font.font64                     (optional)
```

Zip the folder to share all of it; others unzip it into `menu/themes/`. A
share code from the Theme Maker carries the colors, background and pattern
only. Use sounds you made, or whose licence lets you share them.
</details>

<details>
<summary><b>🕹️ Retro games: where their pictures go</b></summary>

NES, SNES and Game Boy games need the menu's emulators in `menu/emulators/`
(see N64FlashcartMenu's documentation). Their pictures are read from the SD
card, one folder per game, named after the game's file without its
extension:

```
menu/metadata/nes/Super Mario Bros. (World)/boxart_front.png
                                            boxart_back.png
                                            boxart_spine.png
                                            gamepak_front.png   (see-through background)
                                            gamepak_back.png
                                            metadata.ini, description.txt
```

`snes`, `gb` and `gbc` work the same way. The Companion's Missing Metadata
tab puts everything in the right place for you.
</details>

<details>
<summary><b>🔐 Checksums</b></summary>

SHA-1 of each `sc64menu.n64` and of the Logos and Clips pack, to check your download:

```
916c8c538ec1f5c6ff321ae26d402edda4fc13e9  sc64menu.n64 (Art Edition)
9745b4e8711bf82033e02df59fc4b0fea9b9a3bd  sc64menu.n64 in SummerStation64-v0.4a-Standard.zip
43ece5bbe6bcef7f65d120b4acbec441baf45aa9  SummerStation64-v0.4a-Logos-and-Clips.zip
```
</details>

---

## ⚠️ Known limitations

- **Profiles:** 64DD disk saves and the Controller Pak are shared by all
  players. Renaming a game on the Folders tab updates the playing profile's
  Recent, Favorites and collections, not the other players'.
- **Edit Theme** changes colors, the background and the pattern; a theme's
  switches, sounds and font are set in the Theme Maker, the Sounds tab or by
  hand. Saving a copy under a new name doesn't copy the original folder's
  sounds or font.
- **A theme's font** applies after a restart, and the big title sizes stay
  the menu's own.
- **Covers can take a moment to appear** while scrolling quickly, and a 64DD
  disk's first cover after power-on takes about a tenth of a second longer
  (its ID is read from the disk). Faster cover loading is planned for v0.5.
- **Going back to an older menu** (v0.3 or the stock N64FlashcartMenu) keeps
  only your first 8 favorites, and drops the rest the next time it saves;
  profiles 2-4's saves stay in `saves/<profile>/` beside your games.
- **Emulator games** (NES, SNES, Game Boy) can't be favorited yet.
- **Cheats:** codes that need a button held (starting D0 to D3) may not work
  yet, and the menu reads up to 64 code lines per game.
- **Tested on:** every build on an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB). Players report it working on the **Analogue 3D**,
  the **ModRetro M64** and **PAL** consoles, thank you!
- **Party mode** has been tested with a stand-in for extra controllers, not
  with four real ones yet. Reports welcome.
- Saves of 64DD disks are not backed up. Other flashcarts are not supported.

---

## 🤖 AI disclosure

SummerStation64 is made with **Claude**, an AI model by Anthropic, used
through **Claude Code** inside Visual Studio Code.

- **Model:** Claude Opus 5.5, as recorded on the commits.
- **How much, in v0.4a:** all 38 commits since v0.3c are co-authored by
  Claude (October 28 to November 10, 2026), including stewx's 11, which were
  also made with Claude Code: about 6,600 changed lines of menu code (14 new
  source files), the Companion's Sounds and Pictures tabs, theme guide and
  other Guide sections, the logo and clip scripts, the tests, and these notes.
- **What the owner (Kerillian) did:** gathered the players' requests,
  decided which to build and how each should behave (profiles and what they
  keep apart, the theme editor's color grid with sliders, the Switch-style
  keyboard, removing Frame Borders, profile pictures), made the sounds and music for their own
  theme, chose the built-in profile pictures, and **tested every build on a real N64 and CRT**. Nothing was
  committed until it passed on the console.
- **Not made by AI:** the music and interface sounds, the fonts, the built-in
  profile pictures, the box art and game details (a community pack), the game data (libretro), and
  N64FlashcartMenu and libdragon, which this is built on.

If something seems wrong, please open an issue.

---

## 💛 Free, always

SummerStation64 is free and always will be, from v0.1 to v1.0 and beyond. I
will never charge for it.

**If you'd like to give money,** please support the people who made it
possible:
- **Polprzewodnikowy**, creator of the SummerCart64:
  [ko-fi.com/polprzewodnikowy](https://ko-fi.com/polprzewodnikowy)
- **networkfusion**, maintainer of N64FlashcartMenu:
  [ko-fi.com/networkfusion](https://ko-fi.com/networkfusion)

Buying your SummerCart64 from an official seller supports Polprzewodnikowy
too.

**If you'd rather give time,** help with the box art and game details every
player sees. The
[N64 flashcart menu metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata#description)
project welcomes new art, descriptions and fixes; its README shows how to
start.

---

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors, and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Title logos, video previews and review scores by **[stewx](https://github.com/stewx)**.
- Requests and reports from players on Reddit, especially **Dispacho**
  (homebrew marks, hidden hacks) and **Quest-Quest** (64DD covers, the
  intro's parts).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever)
  fork.
- Genres, player counts, series, and the lists used by Card Prep and the
  Missing Metadata tab:
  [libretro-database](https://github.com/libretro/libretro-database)
  (CC-BY-SA 4.0).
- Music by Eric Matyas, www.soundimage.org ("Cyber-dream-loop").
  Interface sounds by Lokif (CC0). Title lettering drawn with the Anton and
  Mr Dafoe fonts (SIL Open Font License).

Released under the **GNU Affero General Public License v3**, like
N64FlashcartMenu; the source is in this repository.

> [!NOTE]
> **About the built-in covers and game details:** the Art Edition's covers
> and cartridges, and the game descriptions built into both editions, are
> not part of the source code and not covered by its licence. They belong to
> the games' publishers and writers, and are included (in reduced size) for
> use with the menu on your own console. They come from the community
> [n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
> pack (scans credited there to ABeezy, LaunchBox community). If you hold
> the rights to any of them and want them removed, open an issue and the
> release files will be replaced without them. The same goes for the
> **built-in profile pictures**, which show Nintendo characters: they are
> not part of the source code, belong to Nintendo, and are only inside the
> release files. And for the **title logos and preview clips** in the
> Logos-and-Clips pack: they belong to the games' publishers, are included
> in reduced size for use with the menu on your own console, and are not
> part of the source code. The logos come from libretro's thumbnail
> collection ([libretro-thumbnails](https://github.com/libretro-thumbnails/Nintendo_-_Nintendo_64));
> the clips are cut from gameplay recordings kept on
> [archive.org](https://archive.org). Ask, and they are removed.

<sub>SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.</sub>
