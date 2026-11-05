#!/bin/bash
# ARMv7 build of Kick for the MPC Key 61 in the kick-armv7-builder Docker image (details: ../../build-recipe.md).
# Usage: W=<checkout> CACHE=<writable CPM cache> [DEBUG=1] [BUILD=build-arm] [JOBS=2] bash build-armv7.sh
#   W      checkout (worktree) to build; CACHE  writable CPM source cache (a read-only mount breaks cmake.lock)
#   DEBUG  1 = -g build (~190 MB binary, for gdb-multiarch core analysis); default Release
#   JOBS   parallel jobs; 4 ran out of memory in the container (cc1plus killed), 2 is safe
set -euo pipefail
: "${W:?set W to the checkout}"; : "${CACHE:?set CACHE to a writable CPM cache dir}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BUILD:-build-arm-${DEBUG:+dbg}${DEBUG:-rel}}"; JOBS="${JOBS:-2}"
FLAGS=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/tc/armv7.cmake -DKICK_ENABLE_HARDWARE=ON
       -DKICK_TOUCH_SINGLE_TAP_LISTBOX=ON -DCPM_SOURCE_CACHE=/cache -DTIMESTAMP=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5)
[ -n "${DEBUG:-}" ] && FLAGS+=(-DCMAKE_CXX_FLAGS=-g)
docker run --rm -v "$W":/workspace -v "$CACHE":/cache -v "$HERE":/tc:ro \
  -e PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig -e PKG_CONFIG_SYSROOT_DIR=/ \
  kick-armv7-builder bash -c "set -e; cmake -S Kick -B $BUILD ${FLAGS[*]}; cmake --build $BUILD --target Kick -j$JOBS"
echo "binary: $W/$BUILD/apps/kick/Kick_artefacts/Release/Kick"
