# Themes for the release's SD card zip

`scripts/make_release.py` puts every folder in `assets/themes/source/` that
holds a `theme.ini` into the release's
`SummerStation64-vX-Covers-Logos-and-Clips.zip` as `menu/themes/<folder>/`.
Players who unzip it find the theme under Settings > Look > Theme, listed by
its folder name.

`assets/themes/source/` is ignored by git and never committed: a theme's
music and sounds may belong to someone else. Copy the theme's folder from
the SD card (`menu/themes/<name>/`) as it is. Only the files the menu reads
are packed: `theme.ini` (or `theme.txt`), `font.font64` and the sounds
`music`, `intro`, `move`, `select`, `back`, `setting`, `error` and `tab`
`.wav64`. Anything else in the folder is left out.

The menu file itself carries no card theme: the SummerCart64 must load it
within 5 seconds, and music runs to megabytes.
