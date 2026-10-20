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
> copy it to the top folder of your SD card, and switch on. Covers and game
> details are built in. The release page has the full install guide.

## ✨ Highlights

| | |
|---|---|
| 🎮 **Cover row** | Your games as box covers that slide left and right; turn a box over to see its back. Japanese and 64DD boxes keep their own shape. |
| 🗂️ **Tabs** | Games, Favorites, Folders and Recent, in the order you choose. Games gathers games from as many folders as you like. |
| 🔎 **Find a game** | Filter by region, 64DD or hacks; sort by name, recently played, release date or developer; or press Z for a random game. |
| 🏷️ **Badges** | Players, Expansion Pak, save found, Controller / Rumble / Transfer Pak, and a Hack badge for ROM hacks. |
| 📖 **Game info** | The cover over a blurred copy of itself, a big title, times played, the description, and a ring tinted to match the box. |
| 🎨 **Themes** | Four built-in summer themes, or your own made with the web [Theme Maker](theme-maker/) and shared with a short code. |
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

- **[Release notes and install guide](docs/RELEASE_NOTES_v0.1.md)**: what's
  new, installing, first steps, controls, settings and known limitations.
- **[Theme Maker](theme-maker/)**: build a `theme.ini` in your web browser
  with a live preview.
- **[N64FlashcartMenu's documentation](docs/00_index.md)**: still describes
  the file browser, ROM settings, cheats, 64DD and the other stock features.

## 🧭 Roadmap

- **v0.1** ✅ released: the cover row, tabs, themes, badges, save backups,
  tested on 4MB and 8MB.
- **v0.2** smooth and safe: angled side covers, a lighter background for 4MB,
  a PC prep tool.
- **v0.3** organizing: one cover per game with a version picker for hacks,
  collections, search, screenshots.
- **v0.4** themes: theme picker, sound and font packs, more seasonal themes.
- **v0.5** polish: setup wizard, 240p layout, CRT test patterns, accessibility.

## 💬 Feedback

This is an early project and feedback is very welcome. Please
[open an issue](https://github.com/Kerillian0/SummerStation64/issues/new)
with your console setup (Expansion Pak or Jumper Pak, NTSC or PAL) and, if
something went wrong, a photo of the screen. Reports from PAL consoles are
especially welcome.

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

SummerStation64 is written almost entirely by **Claude** (Anthropic's AI
model, Claude Opus 5.5) through Claude Code, directed by the owner, who
decides what to build, makes every design choice, and tests every build on
a real N64 before it is committed. The
[release notes](docs/RELEASE_NOTES_v0.1.md#-ai-disclosure) have the details:
how much, how the work goes, and what is not made by AI.

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors (their README is kept in
  [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md)), and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever) fork.
- Music by Eric Matyas, www.soundimage.org. Interface sounds by Lokif (CC0).
  Title lettering with the Anton and Mr Dafoe fonts (SIL Open Font License).
- Box art and game details in the release files come from the community
  [n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
  pack; they belong to the games' publishers and are not part of the source
  code or its licence.

Released under the [GNU Affero General Public License v3](LICENSE.md), like
N64FlashcartMenu.

<sub>SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.</sub>
