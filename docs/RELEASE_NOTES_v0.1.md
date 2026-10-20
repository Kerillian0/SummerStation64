# SummerStation64 v0.1

<p align="center">
  <b>☀️ A themeable game menu for the SummerCart64, built on a real N64 and a CRT. ☀️</b><br>
  <img src="https://img.shields.io/badge/SummerCart64-menu-f2b134" alt="SummerCart64 menu">
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

SummerStation64 is an unofficial game menu for the **SummerCart64**, made as
a fork of [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
(based on its V0.3.4). It turns the file list into a row of game covers,
with tabs, themes, badges, save backups and an intro.

> [!TIP]
> **Quick start:** download **`sc64menu.n64`** from the Assets below, copy it
> to the top folder of your SD card, switch on. Covers and game details are
> built in.

> [!IMPORTANT]
> This is a first release. **Back up your SD card**, and keep your current
> `sc64menu.n64` somewhere safe so you can go back.

> [!NOTE]
> **💬 Feedback wanted!** This is the first release, and I'd love to hear
> how it runs on your console: what works, what doesn't, and what you'd
> like to see next. Please
> [open an issue](https://github.com/Kerillian0/SummerStation64/issues/new)
> with your console setup (Expansion Pak or Jumper Pak, NTSC or PAL) and,
> if something went wrong, a photo of the screen. Reports from PAL consoles
> are especially welcome, as none has been tried yet.

---

## ✨ What's new compared with N64FlashcartMenu

### 🎮 Browsing
- **Your games as a row of box covers** that slide left and right, with
  optional smaller covers either side. Up/down turns a box over to its back.
- **Tabs along the top:** Games, Favorites, Folders, and optionally Recent.
  Choose which appear, and in what order, under Settings > Tabs. L and R
  switch between them, and each tab goes back to the game it was left on.
- **Games** shows only games, gathered from as many folders as you like.
- **Filter** the Games tab with C-up / C-down: All, USA, Japan, Europe,
  64DD or Hacks.
- **🎲 Random game:** press Z on Games, Recent or Favorites.
- **Sort** by name, recently played, release date or developer
  (Settings > Library > Sort By).
- **Folders** is a plain file browser for everything else: pictures, music,
  text, zips.
- Optional wrap-around: the last game sits left of the first.

### 🏷️ Game info
- **Badges** under each game's name: players, Expansion Pak, save found, and
  the accessories it uses (Controller Pak, Rumble Pak, Transfer Pak; shown
  as blue boxes, grey words or not at all: Settings > Display > Accessory
  Badges).
- **A screen per game:** the cover over a blurred copy of itself, a large
  title, times played and when, and the description. START shows the
  technical details.
- The ring round the cover takes the colour of the box art.
- Japanese (tall) and 64DD covers keep their own shape.

### 🛠️ ROM hacks
- Hacks get an orange **Hack** badge and their own step in the filter.
- A file counts as a hack when its name says so ("hack", "redux", or a
  `[h1]`-style tag), or when it shares its game code with the original but
  its name is a different game ("Super Mario 64 - Return to Yoshi's Island"
  next to "Super Mario 64"). Two copies of the same game under slightly
  different names are not called hacks.

### 🎨 Look and sound
- **Four built-in summer themes** (Sunset, Night Drive, Beach, Ocean), or
  your own `theme.ini`.
- **An intro** with a spinning logo and a tune, and a fade-in at start-up.
  Both can be changed or turned off.
- Rounded corners, coloured button icons, a favourite heart, and a memory
  badge showing whether an Expansion Pak is fitted.
- Background music chosen to suit the menu.

### 🛡️ Comfort and safety
- **💾 Save backups:** each time you start a game, the menu keeps its save
  from before, by date. Up to five per game, never the same one twice. To
  go back, open the game's Options and choose **Restore save**; the save you
  have is kept too, so you can change your mind.
- **Quick Launch** (tap A to start) or **Hold A To Launch**, if you want
  either.
- **Tidy names:** no file endings, "Legend of Zelda, The" shown as "The
  Legend of Zelda". Your files are never renamed.
- Play counts and "last played".
- **Safe mode:** hold **Z** while switching on to start without your theme
  and settings.
- Settings are saved safely, so switching off at the wrong moment can't
  break them. A friendly screen replaces the crash dump if the menu fails.
- **Low on memory?** The menu leaves a few extras out instead of stopping
  (side cover art, the intro's title picture). Settings > System > Memory
  shows what is free and what was left out.

---

## 📥 Installing

### Choose a download

|  | ⭐ **Art Edition** (recommended) | **Standard** |
|---|---|---|
| **File in the Assets** | `sc64menu.n64` | `SummerStation64-v0.1-Standard.zip` |
| **Size** | about 28 MB | about 3 MB |
| **Box covers** | built in, appear faster | from the metadata pack on your SD card |
| **Game details** (descriptions, dates, developers, players) | built in | built in |
| **Anything else to download?** | no | the metadata pack, for covers |

Both are the same menu. Choose **Standard** if you want to use your own box
art or prefer the small download. You can switch at any time by copying the
other file over it.

### ⭐ Art Edition
1. **Update your SummerCart64's firmware** if it is old.
2. **Back up your menu:** copy `sc64menu.n64` from the top folder of your SD
   card to your computer.
3. **Download `sc64menu.n64`** from the Assets below.
4. **Copy it to the top folder of your SD card**, replacing the old one. The
   name must stay exactly `sc64menu.n64`.
5. **Switch the N64 on.** The intro plays, then the Games tab opens.

### Standard
1. Update the firmware and back up your menu, as above.
2. **Download `SummerStation64-v0.1-Standard.zip`** and unzip it. Inside is
   a file already named `sc64menu.n64`.
3. **Copy that `sc64menu.n64` to the top folder of your SD card.**
4. **Add the metadata pack for covers:** download it from the
   [n64-flashcart-menu-metadata releases](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
   and extract it so its folders sit in `sd:/menu/metadata/` (for example
   `sd:/menu/metadata/N/G/E/E/`). If you used box art with the stock menu,
   it is already there.
5. **Switch the N64 on.**

> [!NOTE]
> With the metadata pack on the card, either edition also shows the box's
> sides and top on the game info screen. To go back to the stock menu, copy
> your backed-up `sc64menu.n64` onto the card the same way.

---

## 🚀 First steps

### Adding your games
The Games tab starts with your **start folder** (the folder the stock menu
opened in). To add more, for example one per region and one for 64DD disks:

1. Press **R** until you reach the **Folders** tab.
2. Go into the folder that holds the games.
3. Press **Z** and choose **Add this folder to Games**.

To take a folder out again, choose **Remove this folder from Games** in the
same place. Settings > Library shows how many folders are added and can
forget them all. To change the start folder itself, go into it on the
Folders tab, press Z and choose **Set current directory as default**.

### Good to know
- **The first browse is the slowest.** The menu notes which file is which
  game (in `sd:/menu/gameindex.txt`), so covers and badges come up faster
  afterwards. Leave the Games tab still for a few seconds and it reads the
  games it doesn't know yet by itself.
- **Region filter:** a game's region comes from its game code, or from tags
  in the file name such as `(U)`, `(J)`, `(E)`, `(USA)` or `(Japan)`. Until
  the menu has read a game, one with no tag in its name shows only under
  "All".
- **Without covers** (Standard edition, no metadata pack) a cover shows the
  game's name. The
  [N64FlashcartMenu box art page](https://github.com/Polprzewodnikowy/N64FlashcartMenu/blob/main/docs/19_gamepak_boxart.md)
  explains how the metadata folder is laid out.

<details>
<summary><b>🕹️ Controls</b></summary>

| Button | Cover tabs (Games, Recent, Favorites) | Folders tab |
|---|---|---|
| Left / Right | previous / next game | |
| Up / Down | turn the box over | move up / down the list |
| C-left / C-right | Games tab: jump 10 games | |
| C-up / C-down | Games tab: change the filter | jump 10 entries |
| A | game info (or start the game, with Quick Launch) | open |
| B | | go up a folder |
| L / R | previous / next tab | previous / next tab |
| Z | options (Random game, and more) | options |
| START | settings | settings |

On the game info screen: A starts the game, B goes back, R opens the game's
options, START shows the details, left/right show the game's other pictures.

</details>

<details>
<summary><b>⚙️ Settings</b></summary>

START opens Settings, with these pages:
- **Display:** theme, covers, animations, badges and on-screen extras.
- **Controls:** launch style, wrap-around and more.
- **Sound:** music and sound effects.
- **Tabs:** which tabs appear and in what order.
- **Library:** sort order, how names are shown, play counts, save backups,
  game folders.
- **Files:** what the file browser shows.
- **System:** intro, fade-in, character set, memory, and a performance
  overlay for testing.

Every page explains the selected setting at the bottom.

</details>

<details>
<summary><b>📁 Files the menu keeps on your SD card</b></summary>

All under `sd:/menu/`. Any of them can be deleted to go back to the
defaults.

| File | What it holds |
|---|---|
| `theme/theme.ini` | your own theme (optional) |
| `features.ini`, `options.ini` | your settings |
| `folders.ini` | the selected game in each folder |
| `gamefolders.txt` | the folders added to the Games tab |
| `playstats.txt` | play counts and last played |
| `gameindex.txt` | which file is which game; rebuilt by itself if deleted |

Kept saves sit beside your saves, in a `backups` folder with one folder per
game (`saves/backups/<game>/` when the menu keeps saves in a `saves`
folder). Delete any you no longer want.

</details>

<details>
<summary><b>🎨 Making your own theme</b></summary>

Put a `theme.ini` in `sd:/menu/theme/`, then choose **From SD Card** under
Settings > Display > Theme. The Theme Maker in this repository
(`theme-maker/theme-maker.html`, open it in any web browser) builds one with
a live preview of the Games and Folders screens, the four built-in themes as
starting points, and share codes.

</details>

---

## ⚠️ Known limitations

- **Tested on one console:** an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB). PAL consoles haven't been tried.
- **On 4MB** the menu picks a smaller character set by itself (Settings >
  System > Character Set: Auto), which leaves plenty of room. The full set
  also works on 4MB but leaves extras out, and Japanese names need it.
- Accessory badges come from the menu's built-in game list; a game it
  doesn't list shows none.
- Saves of 64DD disks are not backed up.
- Other flashcarts (64drive, ED64) are not supported by this fork.

---

## 🤖 AI disclosure

SummerStation64 was made with **Claude**, an AI model by Anthropic, used
through **Claude Code** (Anthropic's coding assistant) inside Visual Studio
Code.

- **Model:** Claude Opus 5.5, as recorded on the commits.
- **How much:** Claude wrote nearly all of this fork's code and text.
  - 121 of the 123 commits between N64FlashcartMenu V0.3.4 and v0.1 are
    co-authored by Claude (October 4 to 20, 2026).
  - About 10,900 lines of menu code in 82 new source files. The original
    menu's own files were changed only with small hooks that call the new
    code.
  - Also the build and art scripts, the PC-side tests, the Theme Maker, and
    the documentation, these release notes included.
- **What the owner (Kerillian) did:** decided what to build and in what
  order, supplied the design mockups, made every choice along the way
  (features, layout, colours, sounds, art and licensing questions), and
  **tested every build on a real N64 and CRT**, with and without an
  Expansion Pak, reporting back with photos and debug logs. Nothing was
  committed until it passed on the console.
- **How the work went:** one change at a time. Claude proposed and wrote a
  change, built it and explained it in plain words; the owner tested it on
  hardware; problems seen there (with the console's own timing and memory
  figures) came back to Claude to fix.
- **Not made by AI:** the music and interface sounds (credited below), the
  fonts, the box art and game details (a community pack), and
  N64FlashcartMenu and libdragon, which this is built on. The icons, the
  title picture and the intro logo are drawn by small programs Claude
  wrote, not by an image-generating AI.

If something seems wrong, please open an issue.

---

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors, and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever)
  fork.
- Music by Eric Matyas, www.soundimage.org ("Cyber-dream-loop").
- Interface sounds by Lokif (CC0), from opengameart.org.
- Title lettering drawn with the Anton and Mr Dafoe fonts (SIL Open Font
  License).

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
