#!/usr/bin/env python3
"""Checks assets/data/whatsnew.txt: its form, lengths, and that every "go"
path names a real Settings row (heading > page > row, or a row's own page
after it), as src/menu/views/settings_menu.c has them. Run by tests/run.sh."""
import os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
top = os.path.dirname(here)
src = open(os.path.join(top, 'src/menu/views/settings_menu.c')).read()

items = {}
for m in re.finditer(r'static const item_t (\w+)\[\] = \{(.*?)\n\};', src, re.S):
    items[m.group(1)] = re.findall(r'(?:SWITCH|CHOICE|FEATURE|CHOICES|ACTION|ACTION_INFO|INFO|SLIDER)\("([^"]+)"', m.group(2))
subs = {}   # a row that opens a page of its own: its label -> that page's rows
for m in re.finditer(r'static const category_t (\w+) = \{ "([^"]+)", (\w+), COUNT', src):
    opener = re.search(r'\w+\("([^"]+)", open_' + m.group(1).replace('_page', '') + r'\w*', src)
    if opener:
        subs[opener.group(1)] = items[m.group(3)]
pages = {}
for group, array in (('Look', 'look_pages'), ('Games', 'games_pages'), ('Console', 'console_pages')):
    body = re.search(r'static const category_t ' + array + r'\[\] = \{(.*?)\n\};', src, re.S).group(1)
    for name, it in re.findall(r'\{ "([^"]+)", (\w+), COUNT', body):
        pages[(group, name)] = items[it]

failures = checks = 0
def check(ok, what):
    global failures, checks
    checks += 1
    if not ok:
        failures += 1
        print('  FAIL:', what)

lines = open(os.path.join(top, 'assets/data/whatsnew.txt')).read().splitlines()
check(lines and lines[0] == 'whatsnew 1', 'the header')
version = None
count = 0
for line in lines[1:]:
    if not line or line.startswith(';'):
        continue
    key, _, rest = line.partition(' ')
    check(key in ('version', 'item', 'go', 'about'), 'a known line: ' + line[:40])
    if key == 'version':
        version = rest
        check(re.fullmatch(r'\d+\.\d+[a-z]?', rest) is not None, 'a version number: ' + rest)
    elif key == 'item':
        count += 1
        check(version is not None, 'an item after a version: ' + rest)
        check(len(rest) <= 30, 'item name of 30 characters at most: ' + rest)
    elif key == 'about':
        check(len(rest) <= 200, 'about of 200 characters at most (%d): %s' % (len(rest), rest[:40]))
    elif key == 'go':
        parts = rest.split(' > ')
        ok = len(parts) in (3, 4) and (parts[0], parts[1]) in pages and parts[2] in pages[(parts[0], parts[1])]
        if ok and len(parts) == 4:
            ok = parts[3] in subs.get(parts[2], [])
        check(ok, 'go path names a Settings row: ' + rest)
check(count >= 1, 'at least one item')
print('whats new list: %d items, %d checks, %d failed' % (count, checks, failures))
sys.exit(1 if failures else 0)
