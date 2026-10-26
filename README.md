<h1 align="center">☀️ SummerStation64 ☀️</h1>

<p align="center">
  <b>A themeable game menu for the SummerCart64, built on a real N64 and a CRT.</b><br><br>
  <a href="https://github.com/Kerillian0/SummerStation64/releases/latest"><img src="https://img.shields.io/github/v/release/Kerillian0/SummerStation64?label=download&color=f2b134" alt="Download the latest release"></a>
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

SummerStation64 is an unofficial game menu for the **SummerCart64** flashcart,
made as a fork of [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu).
It turns the file list into a row of box covers, with tabs, themes, badges,
save backups and an intro, and every build is tested on a real Nintendo 64
and a CRT television, with and without the Expansion Pak.

<!-- Screenshots: drag pictures into GitHub's editor for this file, so they
     are stored on GitHub's image server rather than in the repository. -->

> [!TIP]
> **Quick start:** download **`sc64menu.n64`** from the
> [latest release](https://github.com/Kerillian0/SummerStation64/releases/latest),
> copy it to the top folder of your SD card, switch on. Covers and game
> details are built in.

> [!IMPORTANT]
> This is a first release. **Back up your SD card**, and keep your current
> `sc64menu.n64` somewhere safe so you can go back.

> [!NOTE]
> **💬 Feedback wanted!** This is the first release, and I'd love to hear
> how it runs on your console: what works, what doesn't, and what you'd
> like to see next.
> Please [open an issue](https://github.com/Kerillian0/SummerStation64/issues)
> with your console setup (Expansion Pak or Jumper Pak, NTSC or PAL) and,
> if something went wrong, a photo of the screen.
>--Please test on Analogue 3D! I don't have one and need some testers for it!--
>--I'd like to create an exclusive theme for it. Sounds, Theme, and Analogue 3D check in the top right.--

<img width="640" height="480" alt="shot0004" src="https://github.com/user-attachments/assets/a709d0db-f93b-4a62-a116-7f67c348ba06" />
<img width="640" height="480" alt="shot0003" src="https://github.com/user-attachments/assets/0ef95926-fc83-45f7-9089-28a5c923ce71" />
<img width="640" height="480" alt="shot0005" src="https://github.com/user-attachments/assets/11e1ac76-b359-4dd4-ac19-371a01a089b0" />
<img width="640" height="480" alt="shot0008" src="https://github.com/user-attachments/assets/9d34cba9-8be6-479b-a2c6-5ea67e2143f1" />
<img width="640" height="480" alt="shot0001" src="https://github.com/user-attachments/assets/26081a8c-d606-4dd4-bca0-5e4516004837" />

## ✨ Highlights

| | |
|---|---|
| 🎮 **Cover row** | Your games as 3D boxes or cartridges that hover; hold B to turn one round, or turn it over to see its back. Japanese and 64DD boxes keep their own shape. |
| 🗂️ **Tabs** | Games, Favorites, Folders, Recent, Collections, Homebrew and Retro, up to four in the order you choose. Games gathers games from as many folders as you like. |
| 🔎 **Find a game** | Hold Z for a letter dial; filter by region, 64DD, hacks or party size; sort by name, recently played or added, release date or developer; or a random game. |
| 📚 **Collections** | Series and multiplayer collections found by themselves, and your own, as cards; one cover per game with its hacks and other regions' copies underneath. |
| 🕹️ **More than N64** | NES, SNES and Game Boy games on a Retro tab with their own box shapes; 64DD disks that start with their cartridge. |
| 📸 **Screenshots and clips** | Hold START for a screenshot; record a clip of the menu and turn it into a GIF with the [Companion](https://kerillian0.github.io/SummerStation64/companion/#clips). |
| 🏷️ **Badges** | Players, Expansion Pak, save found, Controller / Rumble / Transfer Pak, a Hack badge, and a ✓ for known good dumps (checked with [Card Prep](https://kerillian0.github.io/SummerStation64/companion/#prep)). |
| 📖 **Game info** | The cover over a blurred copy of itself, a big title, genre, players and co-op/versus, times played, and the full description, scrolling. |
| 🎨 **Themes** | Six built-in themes, or your own made with the [Theme Maker](https://kerillian0.github.io/SummerStation64/companion/#themes) and shared with a short code. |
| 💾 **Save backups** | Each start keeps the game's save from before, dated, five per game; "Restore save" brings one back. |
| 🛡️ **Safe by design** | Safe mode (hold Z at power-on), safe settings saves, a friendly crash screen, and it leaves extras out instead of stopping when memory is short. |
| 🌅 **Intro** | A spinning logo, a title picture and a tune at power-on, then a fade-in. Both optional. |

## 📥 Downloads

Each [release](https://github.com/Kerillian0/SummerStation64/releases) comes in two editions of the same menu:

- **Art Edition** (`sc64menu.n64`, recommended): box covers built in. Copy one file and go.
- **Standard** (`.zip`): a small download that reads box covers from the
  [metadata pack](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
  on your SD card, for people who want their own art.

Both have the game details (descriptions, release dates, developers, players)
built in.

## 📚 Documentation

- **[Release notes and install guide](docs/RELEASE_NOTES_v0.3a.md)**
  ([v0.3](docs/RELEASE_NOTES_v0.3.md), [v0.2](docs/RELEASE_NOTES_v0.2.md), [v0.1](docs/RELEASE_NOTES_v0.1.md)): what's
  new, installing, first steps, controls, settings and known limitations.
- **[SummerStation64 Companion](https://kerillian0.github.io/SummerStation64/companion/)**:
  the Theme Maker, Card Prep (checks your games against known good dumps),
  Hacks, Missing Metadata, Collections, Clips (GIFs), Cheats and a setup guide, in
  your web browser. Source: [docs/companion/](docs/companion/).
- **[Adding box art and details](https://kerillian0.github.io/SummerStation64/companion/#guide-art)**:
  step by step, for games without built-in art, homebrew, 64DD disks and NES,
  SNES and Game Boy games. Also: [64DD disks and their cartridge](https://kerillian0.github.io/SummerStation64/companion/#guide-64dd).
- **[N64FlashcartMenu's documentation](docs/00_index.md)**: still describes
  the file browser, ROM settings, cheats, 64DD and the other stock features.

## 🧭 Roadmap

- **v0.1** ✅ released: the cover row, tabs, themes, badges, save backups,
  tested on 4MB and 8MB.
- **v0.2** ✅ released: the Companion (Theme Maker, Card Prep, Guide),
  dump check marks, full descriptions, players and co-op/versus, new themes,
  angled side covers, 300 KB more free memory.
- **v0.3** ✅ released: 3D boxes and cartridges, collections, the letter
  dial, one cover per game for hacks and other regions' copies, Homebrew and
  Retro (NES, SNES, Game Boy) tabs, 64DD disks with their cartridge, party
  mode, clips turned into GIFs, and four new Companion tabs.
- **v0.3a** ✅ released: from your feedback: 50 favorites, favorite from any
  cover, simpler Z menus, START straight to Settings, a Hold L + R screenshot
  button (8BitDo mod kit), homebrew kept to its tab, easier 64DD pairing, and
  a box art guide.
- **v0.4** themes: theme picker, sound and font packs, more seasonal themes.
- **v0.5** polish: setup wizard, CRT test patterns, accessibility, and a
  versions screen where a game's copies and hacks unfold into a column of
  covers with their names beside them.
- **v1.0:** menu achievements: for finishing games (from time played) and
  whole collections (every Zelda, every Mario Party, ...), unlocking special
  themes, music, sounds, fonts and intros; menu milestones and other fun ones.
- **After v0.5:** extra support for the **ModRetro M64** and **Analogue 3D**:
  32-bit color, 60 frames a second, a layout that uses the safe area of HDMI
  displays, and widescreen if needed.

## 🔧 Building from source

The project builds with [libdragon](https://github.com/DragonMinded/libdragon)
inside the included dev container (`.devcontainer/`, for VS Code):

```sh
git submodule update --init
make sc64          # writes output/sc64menu.n64
sh tests/run.sh    # PC-side tests (gcc and Node.js)
```

Box art and game details can be built into your own copy from a metadata
pack; see [assets/boxart/README.md](assets/boxart/README.md). Never commit
the art itself.

## 🤖 AI disclosure

SummerStation64 was made with Claude, an AI model by Anthropic, used
through Claude Code (Anthropic's coding assistant) inside Visual Studio
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
  tested every build on a real N64 and CRT, with and without an
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
- There are a few placeholder elements, such as the Intro Logo that are AI generated. If this makes you uncomfortable, please contact me and I will rectify it asap. I am looking to commission or create these elements in the next few releases. If you are willing to contribute to this and create some assets for this frontend menu. Please contact my Discord at "kerillian." And we may work something out.
- If you do not want to use this menu due to AI usage, I completely understand and accept this. Please use the stock menu or N64ever. I do not believe either of these utilize or at the very least disclose AI usage. They are far more talented than me. Please support them as this fork uses their code. They are the reason why this was possible in the first place.
- If something seems wrong, please open an issue.

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors (their README is kept in
  [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md)), and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever) fork.
- Music by Eric Matyas, www.soundimage.org. Interface sounds by Lokif (CC0).
  Title lettering with the Anton and Mr Dafoe fonts (SIL Open Font License).
- Genres and player counts come from
  [libretro-database](https://github.com/libretro/libretro-database)
  (CC-BY-SA 4.0); the table built from it, `assets/data/gamedata.txt`, is
  shared under the same licence.
- Box art and game details in the release files come from the community
  [n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
  pack; they belong to the games' publishers and are not part of the source
  code or its licence.

Released under the [GNU Affero General Public License v3](LICENSE.md), like
N64FlashcartMenu.

<sub>SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.</sub>
