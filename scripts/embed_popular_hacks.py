#!/usr/bin/env python3
"""Copies assets/data/popular_hacks.txt into the Companion page
(docs/companion/index.html), so the Hacks tab has the list even when the page
is opened straight from disk. Run after changing the list:

    python3 scripts/embed_popular_hacks.py

tests/hacks_tab_test.js checks that the two are the same.
"""
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parent.parent
source = root / 'assets' / 'data' / 'popular_hacks.txt'
page = root / 'docs' / 'companion' / 'index.html'
start = '<script type="text/plain" id="popularHacksList">'
end = '</script>'

text = source.read_text(encoding='utf-8').replace('\r\n', '\n')
if '</' in text:
    sys.exit('popular_hacks.txt may not contain "</" (it would end the page\'s script tag)')
html = page.read_text(encoding='utf-8')
at = html.find(start)
if at < 0:
    sys.exit('No ' + start + ' in ' + str(page))
stop = html.index(end, at)
html = html[:at + len(start)] + '\n' + text.rstrip('\n') + '\n' + html[stop:]
page.write_text(html, encoding='utf-8')
print('Copied %d lines of the hacks list into %s' % (text.count('\n') + 1, page.relative_to(root)))
