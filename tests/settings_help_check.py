#!/usr/bin/env python3
"""Every Settings help text must fit its panel (settings_menu.c, draw_help_box()).

The panel's text box is 548 px wide and holds 5 lines of the body font
(Firple Bold 15 px, monospaced: 9.23 px a letter, 19 px a line); the menu
drops whatever doesn't fit. On/off rows get a one-line note under their
help, and so do rows shared by every player (shown with 2+ profiles), so
those may have 4 lines of help. Run by tests/run.sh.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WIDTH = 548
ADVANCE = 9.234375
LINES = 5


def wrapped_lines(text):
    lines = 0
    for paragraph in text.split("\n"):
        used, count = 0.0, 1
        for word in paragraph.split(" "):
            width = len(word) * ADVANCE
            if used == 0:
                used = width
            elif used + ADVANCE + width <= WIDTH:
                used += ADVANCE + width
            else:
                count, used = count + 1, width
        lines += count
    return lines


def rows(source):
    for m in re.finditer(r'\b(FEATURE|CHOICES|SWITCH|ACTION_INFO|ACTION|SLIDER|INFO)\(', source):
        kind, i, depth, j = m.group(1), m.end(), 1, m.end()
        while depth:
            c = source[j]
            if c == '"':
                j += 1
                while source[j] != '"':
                    j += 2 if source[j] == "\\" else 1
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            j += 1
        args = source[i:j - 1]
        strings = list(re.finditer(r'"((?:[^"\\]|\\.)*)"', args))
        if len(strings) < 2:
            continue
        k = len(strings) - 1      # the help: the last run of joined string pieces
        while k > 0 and re.fullmatch(r"\s*", args[strings[k - 1].end():strings[k].start()]):
            k -= 1
        if k == 0:
            continue
        help_text = "".join(s.group(1) for s in strings[k:]).replace('\\"', '"').replace("\\n", "\n")
        shared = re.search(r"OPTION_(FONT|AVATAR_SHAPE|CONSOLE_TYPE)\b", args) or "default_folder" in args
        note = 1 if kind in ("FEATURE", "SWITCH") or shared else 0
        yield strings[0].group(1), help_text, note


source = open(os.path.join(ROOT, "src", "menu", "views", "settings_menu.c")).read()
checked = failed = 0
for name, help_text, note in rows(source):
    checked += 1
    needed = wrapped_lines(help_text) + note
    if needed > LINES:
        failed += 1
        print(f"settings help: \"{name}\" needs {needed} lines, the panel holds {LINES}")
print(f"settings help: {checked} rows, {failed} too long")
sys.exit(1 if failed else 0)
