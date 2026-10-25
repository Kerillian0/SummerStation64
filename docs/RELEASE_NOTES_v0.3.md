# SummerStation64 v0.3

<p align="center">
  <b>☀️ A themeable game menu for the SummerCart64, built on a real N64 and a CRT. ☀️</b><br>
  <img src="https://img.shields.io/badge/SummerCart64-menu-f2b134" alt="SummerCart64 menu">
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

**v0.3 "organizing"** turns your covers into 3D boxes and cartridges, and
gives a big library some order: collections, a letter dial to find a game,
one cover per game with its hacks and other regions' copies tucked
underneath, a Homebrew tab, a Retro tab for NES, SNES and Game Boy games,
64DD disks that start with their cartridge, a party mode, and clips of the
menu you can turn into animated GIFs.

> [!TIP]
> **Quick start:** download **`sc64menu.n64`** from the Assets below, copy it
> to the top folder of your SD card, switch on. Covers and game details are
> built in.

> [!NOTE]
> **Coming from v0.2?** Copy the new `sc64menu.n64` over the old one. Your
> settings, favourites, play counts and save backups stay as they are. The
> new tabs (Collections, Homebrew, Retro) are not on the bar until you add
> them in **Settings > Tabs**. **Hide Region Tags** is now on by default
> (names show without "(U)", "(Europe)"); turn it off in Settings > Display
> if you prefer them. **Angled Side Covers** is now off by default (flat side
> covers); a choice you made yourself stays.

> [!IMPORTANT]
> As always: **back up your SD card**, and keep your current `sc64menu.n64`
> so you can go back.

---

## ✨ What's new

### 📦 3D boxes and cartridges
- The selected cover is a **little 3D box**, turned slightly and seen from
  above, with its top label and spine. It **hovers** gently (Settings >
  Display > **Box Hover**: float, sway, both or off).
- **Hold B and use the stick** to turn it round and look at every side.
  Up and down turn it over to the back.
- **Show Games As: Cartridges** shows the cartridge instead of the box, as a
  3D cartridge with its own plastic color, a circuit board in its slot and a
  back to turn over to.
- **3D Side Covers** (optional) makes the covers either side boxes too.
- **Fill Box Edges:** a box with only a top label or a spine shows it on its
  other edges too, turned to fit (Settings > Display; on by default).

### 🗂️ Finding your way around a big library
- **Letter dial:** **hold Z** on the covers and point the stick at a letter
  to jump straight to the games starting with it. Press A to spell further
  ("SU", "SUP"), B to take a letter back. A tap of Z still opens Options.
- **Collections tab** (add it in Settings > Tabs): your games grouped as
  cards, each showing a fan of its covers. **18 series** are found
  automatically (Mario, Zelda, Star Fox, Banjo, ...), and **multiplayer**
  ones by number of players, co-op and versus. **Make your own** from any
  game's Z menu ("Add to collection..."), named with the letter dial, or on
  your computer with the Companion.
- **Game Versions: Grouped** (Settings > Library): an original, its hacks and
  extra copies share **one cover**; A opens a list to choose which to start.
- **Region Copies: One Cover:** a game's USA, Europe and Japan copies share
  one cover too, chosen by your **Preferred Region**. Start another region's
  copy three times in a row and it becomes that game's cover.
- **Sort By Recently Added**, with a **New** badge for a month on games you
  just put on the card.
- **Hide a game** from the cover tabs with Z ("Hide this game"). It stays on
  the card; the Folders tab can show it again.

### 🎮 More kinds of games
- **Homebrew tab:** homebrew games on a shelf of their own.
- **Retro tab:** **NES, SNES, Game Boy and Game Boy Color** games (played
  with the menu's emulators) leave the Games tab for a shelf of their own,
  with C-up / C-down to pick a system. Each system's covers have its own box
  shape, and pictures, spines and cartridges can be added per game.
- **64DD pairing:** an expansion disk starts **together with its
  cartridge** (the F-Zero X Expansion Kit with F-Zero X) when you press A.
  The pair is found automatically, or chosen with Z ("Pair with a
  cartridge..."). Starting one without its cartridge asks first.
- **64DD and Retro game info** now looks like an N64 game's: cover,
  description, developer, date, genre, players, play counts and pictures.
- **Time played** for every game, shown in Game info's Played box.

### 🎉 Party mode
With two or more controllers plugged in, C-up / C-down on the Games tab
offer **Party**: only the games for that many players. Random game picks
among them. Four small controller ports under the clock show which are in
use (Settings > Display > **Controller Marks**).

### 📸 Screenshots and clips
- **Hold START for a second** to take a screenshot with one controller
  (START on controller 2 still works too). Screenshots are now
  **compressed**, at full or half size, with the console's fine dot pattern
  smoothed out or kept (Settings > System).
- **Record Clip** (Settings > System): leave Settings, and after three ticks
  the menu records 5, 10 or 15 seconds of whatever you do. It saves frame by
  frame, so the console runs slowly while recording, but the clip plays back
  at full speed. The Companion turns it into an **animated GIF**.

### 🧰 The Companion
**[kerillian0.github.io/SummerStation64/companion](https://kerillian0.github.io/SummerStation64/companion/)**
has four new tabs:
- **Hacks:** give a hack its own name, cover and details, so it shows as a
  game of its own; suggestions from a list of over 100 popular hacks.
- **Missing Metadata:** on the console, Settings > Library > **Find Missing
  Metadata** lists what your games lack. This tab fills it in: pictures from
  your own files, developer, date, genre, players and descriptions, also for
  64DD and Retro games, with a **fit editor** (zoom, move and turn each
  picture in the exact shape of its face on the 3D box) and an option to use
  a top label or spine for the box's other edges too. It can fill details by
  itself from **libretro's
  database**, or from a **LaunchBox `Metadata.zip`** you download yourself.
- **Collections:** make and edit your own collections with a keyboard.
- **Clips:** turn a recorded clip into a GIF.

### 🛠️ Also new
- Confirmation prompts show the action in red and **Keep** in green.
- Names hide `[!]` marks and revision numbers too, with Hide Game Extensions.

---

## 📥 Installing

|  | ⭐ **Art Edition** (recommended) | **Standard** |
|---|---|---|
| **File in the Assets** | `sc64menu.n64` | `SummerStation64-v0.3-Standard.zip` |
| **Size** | about 33 MB | about 3 MB |
| **Box covers and cartridges** | built in, appear faster | from the metadata pack on your SD card |
| **Game details** (descriptions, dates, developers, genres, players) | built in | built in |

1. **Update your SummerCart64's firmware** if it is old, and **back up** your
   `sc64menu.n64`.
2. **Art Edition:** copy `sc64menu.n64` from the Assets to the top folder of
   your SD card. **Standard:** unzip `SummerStation64-v0.3-Standard.zip` and
   copy the `sc64menu.n64` inside; for covers, put the
   [metadata pack](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
   in `sd:/menu/metadata/`.
3. **Switch the N64 on.**

New to SummerStation64? The Companion's
**[Guide](https://kerillian0.github.io/SummerStation64/companion/#guide-setup)**
walks through setting up a card, including adding your game folders.

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
<summary><b>🎞️ Making a GIF</b></summary>

1. On the console: Settings > System, choose **Clip Length**, **Clip Size**
   and **Clip Frame Rate**, then press A on **Record Clip** and leave
   Settings.
2. After three ticks it records; a sound says when it is done. Half size at
   15 frames a second records a 10-second clip in about a minute and a half;
   full size at 30 takes about ten minutes.
3. Put the card in your computer, open the
   [Companion's Clips tab](https://kerillian0.github.io/SummerStation64/companion/#clips),
   choose the card and the clip, and press **Make GIF**.
</details>

<details>
<summary><b>🔐 Checksums</b></summary>

SHA-1 of each `sc64menu.n64`, to check your download:

```
cda883d73cc7e4417c3ca36b90ab5669ce471c4d  sc64menu.n64 (Art Edition)
c6c758ade5115f9089f897c5988f7035db8afff3  sc64menu.n64 in SummerStation64-v0.3-Standard.zip
```
</details>

---

## ⚠️ Known limitations

- **Tested on:** every build on an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB). Players have reported earlier versions working
  on the **Analogue 3D** and the **ModRetro M64**, thank you! PAL consoles
  haven't been reported yet; reports from any setup are very welcome.
- **Party mode** has been tested with a stand-in for extra controllers, not
  with four real ones yet. Reports welcome.
- **Recording a clip is slow** (each frame is saved as it is drawn), and the
  controls feel sluggish meanwhile; the clip itself is smooth.
- **Retro games' pictures** are read from the SD card (not built into the
  menu file), so they appear a little later than N64 covers.
- With Game Versions on Grouped, **Pokémon Stadium** and the **Snap Station**
  kiosk version share one cover (they share a game code).
- **LaunchBox** publishes no terms for reusing its database, so the
  Companion only reads a `Metadata.zip` you download yourself, in your
  browser; nothing from it is included with SummerStation64.
- Saves of 64DD disks are not backed up. Other flashcarts are not supported.

---

## 🤖 AI disclosure

SummerStation64 is made with **Claude**, an AI model by Anthropic, used
through **Claude Code** inside Visual Studio Code.

- **Model:** Claude Opus 5.5, as recorded on the commits.
- **How much, in v0.3:** 56 of the 57 commits since v0.2 are co-authored by
  Claude (October 22 to 25, 2026): about 8,000 lines of menu code (17 new
  source files), the Companion's new tabs, the data scripts, the tests, and
  the documentation, these notes included. The other was the owner's own
  edit to the README on GitHub.
- **What the owner (Kerillian) did:** chose what v0.3 should hold and in what
  order, designed how each feature behaves (the letter dial, collections,
  region copies, 64DD pairing, party mode, the Retro tab and clips among
  them), made every decision along the way, and **tested every build on a
  real N64 and CRT**, reporting back with photos and debug logs. Nothing was
  committed until it passed on the console.
- **The data:** the series list was put together by Claude from libretro's
  game names and checked by the owner; the popular hacks list was drafted by
  Claude and is being checked by the owner.
- **Not made by AI:** the music and interface sounds, the fonts, the box art
  and game details (a community pack), the game data (libretro), and
  N64FlashcartMenu and libdragon, which this is built on. Icons, the
  cartridge back and the title picture are drawn by small programs Claude
  wrote, not by an image-generating AI.

If something seems wrong, please open an issue.

---

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors, and on
  [libdragon](https://github.com/DragonMinded/libdragon).
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
> release files will be replaced without them.

<sub>SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.</sub>
