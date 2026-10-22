# SummerStation64 v0.2

<p align="center">
  <b>☀️ A themeable game menu for the SummerCart64, built on a real N64 and a CRT. ☀️</b><br>
  <img src="https://img.shields.io/badge/SummerCart64-menu-f2b134" alt="SummerCart64 menu">
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

**v0.2 "smooth and safe"** brings a web Companion with a Card Prep tool,
check marks for known good dumps, full game descriptions, players and
co-op/versus, new themes, angled side covers, and 300 KB more free memory.
It is now based on N64FlashcartMenu **V0.3.5**.

> [!TIP]
> **Quick start:** download **`sc64menu.n64`** from the Assets below, copy it
> to the top folder of your SD card, switch on. Covers and game details are
> built in.

> [!NOTE]
> **Coming from v0.1?** Copy the new `sc64menu.n64` over the old one. Your
> settings, favourites, play counts and save backups stay as they are.
> The built-in **Night Drive** theme is replaced by **Midnight**, which is
> now the default look (Sunset is still in the list); your own `theme.ini`
> still comes first.

> [!IMPORTANT]
> As always: **back up your SD card**, and keep your current `sc64menu.n64`
> so you can go back.

---

## ✨ What's new

### 🧰 The SummerStation64 Companion
**[kerillian0.github.io/SummerStation64/companion](https://kerillian0.github.io/SummerStation64/companion/)**,
one web page with three tabs, nothing to install:
- **Theme Maker:** design a theme with a live preview of the menu, save it
  as `theme.ini`, or share it as a short code.
- **Card Prep:** checks every game on your SD card against No-Intro's list of
  known good dumps and saves the result for the menu (see below). Your games
  never leave your computer.
- **Guide:** SD card setup, hardware tips (Expansion Pak or Jumper Pak, SD
  cards, TV connections), handy tools, and troubleshooting.

### ✅ Dump check marks
After running Card Prep, each game gets a small mark in the corner of its
title panel and on its info screen:

| Mark | Meaning |
|---|---|
| 🟢 ✓ **Verified** | exactly a known good dump |
| 🟠 ? **Not verified** | the file differs from the known dump. Often harmless (an old tool changed a header byte, or the file is padded); if the game misbehaves, try another copy |
| 🔴 ✗ **Bad dump** | the file name marks it as a bad dump (`[b]`) |

Hacks, translations and fixed dumps get no mark. Turn the marks off under
Settings > Display > **Dump Check Marks**.

### 📖 Game info
- **Full descriptions.** The metadata pack's short descriptions stop
  mid-sentence; v0.2 shows each game's whole description, built into the
  menu (278 games).
- **Long descriptions scroll** slowly after 10 seconds, or straight away with
  the D-Pad or stick (Settings > Display > **Scroll Descriptions**).
- **Players and how they play together:** "1-4, versus", "1-4, co-op" or
  "1-4, co-op/vs", and a **players badge** on the cover rows. The menu knew
  no player counts before.
- **Genre** next to the release date and developer.

### 🎨 Look
- **New built-in themes:** **Midnight** (the new default), **Mint Grid** and
  **Grape**, alongside Sunset, Beach and Ocean.
- **Angled side covers:** with Previous/Next Covers on, the side covers turn
  away like boxes on a shelf and sit further from the screen's edge
  (Settings > Display > **Angled Side Covers**; off gives flat covers).
- **Lighter backgrounds:** themes are now drawn fresh every frame instead of
  being kept as a 300 KB picture. That frees **300 KB of memory** (most
  welcome on 4MB) and the graphics chip has a little more time to spare.

### 🛠️ Also new
- **Screenshots:** press START on a controller in **port 2** to save the
  screen to `menu/screenshots/` (Settings > System > Screenshots).
- The Hack badge and dump marks now show on the **Recent** and **Favorites**
  tabs too.
- **From N64FlashcartMenu V0.3.5:** a message at start-up when the cart
  reports too low a supply voltage, autoload fixes, and newer libdragon and
  libraries.
- **Bug reports:** [New issue](https://github.com/Kerillian0/SummerStation64/issues/new/choose)
  now opens a short form that asks for your setup.

---

## 📥 Installing

|  | ⭐ **Art Edition** (recommended) | **Standard** |
|---|---|---|
| **File in the Assets** | `sc64menu.n64` | `SummerStation64-v0.2-Standard.zip` |
| **Size** | about 27 MB | about 3 MB |
| **Box covers** | built in, appear faster | from the metadata pack on your SD card |
| **Game details** (descriptions, dates, developers, genres, players) | built in | built in |

1. **Update your SummerCart64's firmware** if it is old, and **back up** your
   `sc64menu.n64`.
2. **Art Edition:** copy `sc64menu.n64` from the Assets to the top folder of
   your SD card. **Standard:** unzip `SummerStation64-v0.2-Standard.zip` and
   copy the `sc64menu.n64` inside; for covers, put the
   [metadata pack](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
   in `sd:/menu/metadata/`.
3. **Switch the N64 on.**

New to SummerStation64? The Companion's
**[Guide](https://kerillian0.github.io/SummerStation64/companion/#guide-setup)**
walks through setting up a card, including adding your game folders.

<details>
<summary><b>✅ Checking your games with Card Prep</b></summary>

1. Put the SD card in your computer and open the
   [Companion's Card Prep tab](https://kerillian0.github.io/SummerStation64/companion/#prep)
   in **Chrome or Edge**.
2. Click **Choose SD card** and pick the card's top folder (allow access).
3. Click **Check games.** Every game is read in full, so a full card takes a
   minute or two; next time only new games are read.
4. Click **Save to card.** This writes `menu/verified.txt`, which the menu
   reads at start-up.

Firefox and Safari can check the card too, then give you `verified.txt` to
copy into the card's `menu` folder yourself.
</details>

<details>
<summary><b>🔐 Checksums</b></summary>

SHA-1 of each `sc64menu.n64`, to check your download:

```
5d2e577c2d7e049655b1c09a1ac933f6150afb7e  sc64menu.n64 (Art Edition)
2814852fc7836381e1ed6864629f0d1ce45b667a  sc64menu.n64 in SummerStation64-v0.2-Standard.zip
```
</details>

---

## ⚠️ Known limitations

- **Tested on:** every build on an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB). Players have reported v0.1 working on the
  **Analogue 3D** and the **ModRetro M64**, thank you! PAL consoles haven't
  been reported yet; reports from any setup are very welcome.
- **Players and genres** come from libretro's database and may be wrong for a
  few games. The co-op/versus notes were written by hand for about 40
  multiplayer games so far; others show the player count only. Corrections
  are welcome in an issue.
- **Card Prep** needs Chrome or Edge to save straight onto the card.
- The **Grape** theme's checker pattern can't be drawn live, so it keeps the
  older 300 KB background picture (like Ocean). On a 4MB console with the
  Full character set these two leave the least memory to spare; the other
  themes are the lighter choice there.
- A few Japanese games share a code with another region's game and can show
  its cover (for example Dezaemon 3D). A fix is planned.
- Saves of 64DD disks are not backed up. Other flashcarts are not supported.

---

## 🤖 AI disclosure

SummerStation64 is made with **Claude**, an AI model by Anthropic, used
through **Claude Code** inside Visual Studio Code.

- **Model:** Claude Opus 5.5, as recorded on the commits.
- **How much, in v0.2:** 27 of the 29 commits since v0.1 are co-authored by
  Claude (October 20 to 21, 2026): about 1,250 lines of menu code (6 new
  source files), the Companion page, the build and data scripts, the tests,
  and the documentation, these notes included. Two were the owner's own
  edits to the README on GitHub.
- **What the owner (Kerillian) did:** chose what v0.2 should hold and in what
  order, made every decision along the way (the dump marks' colours and
  wording, the themes, the Companion's tabs and guides, what to credit and
  how), and **tested every build on a real N64 and CRT**, reporting back
  with photos and debug logs. Nothing was committed until it passed on the
  console.
- **The data:** genres and player counts come from libretro-database; the
  co-op/versus notes were drafted by Claude and checked by the owner.
- **Not made by AI:** the music and interface sounds, the fonts, the box art
  and game details (a community pack), the game data (libretro), and
  N64FlashcartMenu and libdragon, which this is built on. Icons and the title
  picture are drawn by small programs Claude wrote, not by an
  image-generating AI.

If something seems wrong, please open an issue.

---

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors, and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever)
  fork.
- Genres, player counts, and No-Intro's list used by Card Prep:
  [libretro-database](https://github.com/libretro/libretro-database)
  (CC-BY-SA 4.0).
- Music by Eric Matyas, www.soundimage.org ("Cyber-dream-loop").
  Interface sounds by Lokif (CC0). Title lettering drawn with the Anton and
  Mr Dafoe fonts (SIL Open Font License).

Released under the **GNU Affero General Public License v3**, like
N64FlashcartMenu; the source is in this repository.

> [!NOTE]
> **About the built-in covers and game details:** the Art Edition's covers,
> and the game descriptions built into both editions, are not part of the
> source code and not covered by its licence. They belong to the games'
> publishers and writers, and are included (covers in reduced size) for use
> with the menu on your own console. They come from the community
> [n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
> pack (scans credited there to ABeezy, LaunchBox community). If you hold
> the rights to any of them and want them removed, open an issue and the
> release files will be replaced without them.

<sub>SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.</sub>
