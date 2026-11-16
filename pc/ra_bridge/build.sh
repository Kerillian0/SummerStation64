#!/bin/sh
# Builds ra_bridge.exe (the RetroAchievements experiment's PC program) for
# Windows in the dev container: sh pc/ra_bridge/build.sh
# Needs the mingw-w64 cross-compiler (sudo apt-get install mingw-w64).
# One .exe, linked statically: nothing else to copy alongside it.
set -e
cd "$(dirname "$0")"
mkdir -p ../../output
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -Werror -std=c11 -static \
    main.c sc64_link.c hook_ring.c game.c rcheevos/src/util/md5.c \
    -lsetupapi -o ../../output/ra_bridge-build.exe
# Windows keeps a running .exe locked: if the old one is still open, the new one is left as ra_bridge-new.exe.
if mv -f ../../output/ra_bridge-build.exe ../../output/ra_bridge.exe 2>/dev/null; then
    ls -l ../../output/ra_bridge.exe
else
    mv -f ../../output/ra_bridge-build.exe ../../output/ra_bridge-new.exe
    echo "output/ra_bridge.exe is in use (still running?): the new build is output/ra_bridge-new.exe"
fi
