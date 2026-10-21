#!/usr/bin/env python3
"""Builds assets/data/gamedata.txt: genre, most players and how they play
together, per game code.

Genre and most players come from libretro-database (CC-BY-SA 4.0,
https://github.com/libretro/libretro-database), keyed there by each dump's
CRC-32; its No-Intro list gives each dump's game code, so the two are joined
by code. Where a code's dumps disagree, the most common genre and the
highest player count win. How they play together (co-op, versus) comes from
play_modes.txt beside this script, written for this project, which also
corrects libretro where it is plainly wrong (P=players, G=genre).

Run from the project's top folder (needs the internet):
    python3 assets/data/make_game_data.py
The result is committed; the build copies it into the menu file.
"""
import collections
import os
import re
import urllib.request

BASE = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/"
LISTS = {
    "nointro": BASE + "no-intro/Nintendo%20-%20Nintendo%2064.dat",
    "genre": BASE + "genre/Nintendo%20-%20Nintendo%2064.dat",
    "users": BASE + "maxusers/Nintendo%20-%20Nintendo%2064.dat",
}
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "gamedata.txt")
MODES = os.path.join(HERE, "play_modes.txt")

# libretro's genre names, shortened for a CRT (and kept to the menu's Latin font).
GENRE_NAMES = {
    "Role-playing (RPG)": "RPG",
    "Hunting and Fishing": "Hunting & Fishing",
    "Beat'em Up": "Beat 'em up",
    "Shoot'em Up": "Shoot 'em up",
    "Music / Dancing": "Music",
    "Sports with Animals": "Sports",
    "Casual Game": "Casual",
}


def fetch(url):
    with urllib.request.urlopen(url) as r:
        return r.read().decode("utf-8", errors="replace")


def games(text):
    """Each game ( ... ) block of a clrmamepro list."""
    return re.findall(r"^game \($(.*?)^\)", text, re.S | re.M)


def by_crc(text, field):
    out = {}
    for block in games(text):
        value = re.search(r'^\s*' + field + r' "?([^"\n]*)"?\s*$', block, re.M)
        crc = re.search(r"crc ([0-9A-Fa-f]{8})", block)
        if value and crc:
            out[crc.group(1).lower()] = value.group(1).strip()
    return out


def main():
    lists = {name: fetch(url) for name, url in LISTS.items()}
    code_of = {}
    for block in games(lists["nointro"]):
        for rom in re.finditer(r'^\s*rom \( (.*) \)\s*$', block, re.M):
            crc = re.search(r"crc ([0-9A-Fa-f]{8})", rom.group(1))
            serial = re.search(r'serial "([A-Z0-9]{4})"', rom.group(1))
            if crc and serial:
                code_of[crc.group(1).lower()] = serial.group(1)
    genre_by_crc = by_crc(lists["genre"], "genre")
    users_by_crc = by_crc(lists["users"], "users")

    genres = collections.defaultdict(collections.Counter)
    players = collections.defaultdict(int)
    for crc, code in code_of.items():
        if crc in genre_by_crc:
            g = genre_by_crc[crc]
            genres[code][GENRE_NAMES.get(g, g)] += 1
        if crc in users_by_crc and users_by_crc[crc].isdigit():
            players[code] = max(players[code], int(users_by_crc[crc]))

    modes = {}
    fixes = {}      # code -> {"P": players, "G": genre}
    with open(MODES, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            code, mode, *rest = line.split()
            modes[code] = mode
            fix = {}
            for token in rest:
                key, _, value = token.partition("=")
                fix[key] = value.replace("_", " ")
            if fix:
                fixes[code] = fix

    # A three-letter key counts for every region of that game.
    for key in [k for k in modes if len(k) == 3]:
        mode = modes.pop(key)
        for code in set(genres) | set(players):
            if code.startswith(key):
                modes.setdefault(code, mode)
        modes.setdefault(key + "E", mode)

    for key, fix in fixes.items():
        for code in set(genres) | set(players) | {key + "E"}:
            if code.startswith(key):
                if "P" in fix:
                    players[code] = int(fix["P"])
                if "G" in fix:
                    genres[code] = collections.Counter({fix["G"]: 1})

    codes = sorted(set(genres) | set(players) | set(modes))
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("gamedata 1\n")
        f.write("# code, most players (0 = not known), play together (- none, C co-op, V versus, B both), genre\n")
        f.write("# Genre and players: libretro-database, CC-BY-SA 4.0. Play modes: play_modes.txt.\n")
        for code in codes:
            genre = genres[code].most_common(1)[0][0] if genres[code] else ""
            f.write(f"{code} {players.get(code, 0)} {modes.get(code, '-')} {genre}".rstrip() + "\n")
    print(f"{len(codes)} game codes: {sum(1 for c in codes if genres[c])} with a genre, "
          f"{sum(1 for c in codes if players.get(c))} with a player count, {len(modes)} with play modes.")


main()
