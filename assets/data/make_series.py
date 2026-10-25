#!/usr/bin/env python3
"""Builds assets/data/series.txt: which game codes belong to which series,
for the menu's Collections tab.

The series and how to recognise their games are ours (series_list.txt
beside this script). The game names and codes come from libretro-database's
No-Intro list (CC-BY-SA 4.0, https://github.com/libretro/libretro-database),
so each series holds every region's exact code (NDZE Duke Nukem and NDZJ
Dezaemon 3D share three letters, so codes are never shortened). A series
needs two different games or more to be kept.

Run from the project's top folder (needs the internet):
    python3 assets/data/make_series.py
The result is committed; the build copies it into the menu file.
"""
import os
import re
import urllib.request

URL = ("https://raw.githubusercontent.com/libretro/libretro-database/master/"
       "metadat/no-intro/Nintendo%20-%20Nintendo%2064.dat")
HERE = os.path.dirname(os.path.abspath(__file__))
LIST = os.path.join(HERE, "series_list.txt")
OUT = os.path.join(HERE, "series.txt")


def main():
    text = urllib.request.urlopen(URL).read().decode("utf-8", errors="replace")
    names = {}      # code -> one of its names
    for block in re.findall(r"^game \($(.*?)^\)", text, re.S | re.M):
        name = re.search(r'^\s*name "([^"]*)"', block, re.M)
        serial = re.search(r'serial "([A-Z0-9]{4})"', block)
        if name and serial:
            names.setdefault(serial.group(1), name.group(1))

    series = []
    with open(LIST, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line and not line.startswith("#"):
                title, pattern = line.split("\t", 1)
                series.append((title, re.compile(pattern)))

    members = [[] for _ in series]
    for code in sorted(names):
        for i, (title, pattern) in enumerate(series):
            if pattern.search(names[code]):
                members[i].append(code)
                break

    kept = []
    for (title, _), codes in zip(series, members):
        games = {code[:3] for code in codes}    # regions of one game count once
        if len(games) >= 2:
            kept.append((title, codes))
        else:
            print(f"left out (one game only): {title}")

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("series 1\n")
        f.write("# A series, then its game codes. Names and codes: libretro-database\n")
        f.write("# (No-Intro), CC-BY-SA 4.0. The series: series_list.txt.\n")
        for title, codes in kept:
            f.write(f"= {title}\n")
            for i in range(0, len(codes), 12):
                f.write(" ".join(codes[i:i + 12]) + "\n")
    for title, codes in kept:
        print(f"{title}: {len(codes)} codes, {len({c[:3] for c in codes})} games")


main()
