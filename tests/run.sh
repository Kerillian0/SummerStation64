#!/bin/sh
# PC-side tests: the theme.ini reader and the hack finder (C, built with the
# PC's own compiler), the Theme Maker's share codes, the Card Prep page and the Hacks tab
# (JavaScript, run with Node.js).
# Run from anywhere:  sh tests/run.sh
# Needs gcc and node (in the dev container: sudo apt-get install -y nodejs).
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

status=0

echo "== theme.ini reader =="
gcc -std=gnu11 -Wall -Werror -I"$here/stubs" -I"$here/../src/menu" \
    -o "$out/theme_parse_test" "$here/theme_parse_test.c" "$here/../src/menu/theme_parse.c"
"$out/theme_parse_test" || status=1

echo
echo "== hacks told from originals =="
gcc -std=gnu11 -Wall -Werror -I"$here/../src/menu" \
    -o "$out/rom_hacks_test" "$here/rom_hacks_test.c" "$here/../src/menu/rom_hacks.c"
"$out/rom_hacks_test" || status=1

echo
echo "== share codes =="
if command -v node >/dev/null 2>&1; then
    node "$here/share_code_test.js" || status=1
    echo
    echo "== card prep page =="
    node "$here/prep_tool_test.js" || status=1
    echo
    echo "== hacks tab =="
    node "$here/hacks_tab_test.js" || status=1
else
    echo "node not found: skipped (sudo apt-get install -y nodejs)"
    status=1
fi

echo
[ $status -eq 0 ] && echo "All tests passed." || echo "Some tests failed."
exit $status
