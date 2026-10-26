# SummerStation64 v0.3a

<p align="center">
  <b>☀️ A themeable game menu for the SummerCart64, built on a real N64 and a CRT. ☀️</b><br>
  <img src="https://img.shields.io/badge/SummerCart64-menu-f2b134" alt="SummerCart64 menu">
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

**v0.3a** is an update made from your feedback on v0.3: 50 favorites, a
favorite button on every cover, simpler menus, START straight to Settings,
a screenshot button that suits the 8BitDo mod kit, homebrew kept to its own
tab if you like, easier 64DD pairing, and a step-by-step guide to adding box
art. Thank you to everyone who tried v0.3 and wrote in!

New here? Everything v0.3 brought (3D boxes and cartridges, collections, the
letter dial, the Retro and Homebrew tabs, party mode, clips) is in the
**[v0.3 release notes](RELEASE_NOTES_v0.3.md)**.

> [!TIP]
> **Quick start:** download **`sc64menu.n64`** from the Assets below, copy it
> to the top folder of your SD card, switch on. Covers and game details are
> built in.

> [!NOTE]
> **Coming from v0.3?** Copy the new `sc64menu.n64` over the old one; your
> settings, favorites, collections and save backups stay. **START now opens
> Settings at once.** What the old START menu held is in Settings: **Information**
> (menu, flashcart and N64 information) and **System** (Controller Pak Manager,
> Clock).

---

## ✨ What's new

### ⭐ Favorites
- **Up to 50 favorites** (was 8).
- **Favorite a game from its cover:** press **Z** on any cover tab and choose
  **Add to Favorites** (or **Remove from Favorites**). No need to open Game
  info first.

### 🧭 Simpler menus
- **Z on a cover** (Games, Homebrew, Retro, Recent, Favorites and inside a
  collection) now offers just: **Random game, Add to Favorites, Game info,
  Add to collection, Hide this game.** File tools (delete, properties, the
  folder options) stay on the **Folders** tab, where they belong.
- **Game info** opens the right page for 64DD disks and Retro games from Z too.
- **START opens Settings straight away.** A new **Information** category holds
  the menu, flashcart and N64 information screens; the **Controller Pak
  Manager** and the **Clock** are in **System**. Each comes back to Settings.
- **Advanced Game Options** (Settings > System, off by default): Set CIC Type,
  Set Save Type, Set TV Type, the Datel Code Editor and Clear RDRAM on boot
  leave Game info's Options unless you switch this on. Choices you made before
  still apply.

### 📸 Screenshot button
- **Settings > System > Screenshot Button:** **Hold START** (as before) or
  **Hold L + R** for a second, for controllers whose START is their power
  button, such as the **8BitDo mod kit**. With L + R, L and R switch tabs when
  you let go, so holding both never flicks between tabs.
- Every controller port now works the same way (port 2's instant START
  screenshot from testing is gone).

### 🍺 Homebrew
- **Settings > Library > Homebrew on Games: Hide** keeps homebrew on the
  Homebrew tab only. Folders, Recent, Favorites and your own collections still
  show it. **Show** stays the default.

### 💾 64DD disks
- **Pair a disk where you are:** press **Z** on the disk's Game info (or on the
  "needs its cartridge" warning) to choose its cartridge. The list names what
  **Automatic** finds and puts the matching cartridges first, in blue.
- The disk's info page says what it plays with: **Plays with F-Zero X** (green),
  **Disk alone** (grey) or **Needs (not found)** (orange).
- **L** (or Options, **Play the disk alone**) starts the disk without its
  cartridge.

### 🧰 The Companion
- **Missing Metadata** finds `menu/missingart.txt` whichever folder you pick
  (the top of the card, or the `menu` folder) and works better in **Firefox**.
- New **Guide** sections:
  **[Adding box art and details, step by step](https://kerillian0.github.io/SummerStation64/companion/#guide-art)**
  and **[64DD disks and their cartridge](https://kerillian0.github.io/SummerStation64/companion/#guide-64dd)**.

---

## 📥 Installing

|  | ⭐ **Art Edition** (recommended) | **Standard** |
|---|---|---|
| **File in the Assets** | `sc64menu.n64` | `SummerStation64-v0.3a-Standard.zip` |
| **Size** | about 35 MB | about 2.5 MB |
| **Box covers and cartridges** | built in, appear faster | from the metadata pack on your SD card |
| **Game details** (descriptions, dates, developers, genres, players) | built in | built in |

1. **Update your SummerCart64's firmware** if it is old, and **back up** your
   `sc64menu.n64`.
2. **Art Edition:** copy `sc64menu.n64` from the Assets to the top folder of
   your SD card. **Standard:** unzip `SummerStation64-v0.3a-Standard.zip` and
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
e829da39207a23bc6277f215770ed4bfaa5fb6cc  sc64menu.n64 (Art Edition)
7b7af4fee2612ecf33000d238bd0f9119ce81c1c  sc64menu.n64 in SummerStation64-v0.3a-Standard.zip
```
</details>

---

## ⚠️ Known limitations

- **Going back to an older menu** (v0.3 or the stock N64FlashcartMenu) keeps
  only your first 8 favorites, and drops the rest the next time it saves.
- **Emulator games** (NES, SNES, Game Boy) can't be favorited: the Favorites
  list has no place for them yet.
- **Tested on:** every build on an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB). Players report it working on the **Analogue 3D**,
  the **ModRetro M64** and **PAL** consoles, thank you!
- **Party mode** has been tested with a stand-in for extra controllers, not
  with four real ones yet. Reports welcome.
- **Recording a clip is slow** (each frame is saved as it is drawn); the clip
  itself is smooth.
- With Game Versions on Grouped, **Pokémon Stadium** and the **Snap Station**
  kiosk version share one cover (they share a game code).
- Saves of 64DD disks are not backed up. Other flashcarts are not supported.

---

## 🤖 AI disclosure

SummerStation64 is made with **Claude**, an AI model by Anthropic, used
through **Claude Code** inside Visual Studio Code.

- **Model:** Claude Opus 5.5, as recorded on the commits.
- **How much, in v0.3a:** all 8 commits since v0.3 are co-authored by Claude
  (October 25 and 26, 2026): about 600 changed lines of menu code (one new
  source file), the Companion's fixes and Guide sections, and these notes.
- **What the owner (Kerillian) did:** gathered the players' requests,
  decided which to build and how each should behave, edited the popular
  hacks list, and **tested every build on a real N64 and CRT**. Nothing was
  committed until it passed on the console.
- **Not made by AI:** the music and interface sounds, the fonts, the box art
  and game details (a community pack), the game data (libretro), and
  N64FlashcartMenu and libdragon, which this is built on.

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
