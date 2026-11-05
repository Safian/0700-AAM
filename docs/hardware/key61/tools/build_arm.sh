#!/bin/sh
# Build the Kick for the Akai MPC Key 61 (ARMv7) in the kick-armv7-builder Docker image.
# Recipe: docs/AKAI_MPC_KEY61_BUILD_RECIPE.md. A full build takes about 4-5 minutes; run it in the background.
#
# Usage: build_arm.sh [--dry-run]
# Environment (all optional):
#   WORKTREE       the checkout to build (default: the git top level of the current directory)
#   CPM_CACHE      writable dependency cache (default: $HOME/.cache/kick-cpm; a read-only mount makes CPM fail on cmake.lock)
#   TOOLCHAIN_DIR  folder holding armv7.cmake (default: a temporary folder, the file is created from the recipe)
#   JOBS           compile jobs (default: 2). Do NOT use -j4: cc1plus is killed for lack of memory in the 8 GB Docker VM.
# Result: build-arm-cpm/apps/kick/Kick_artefacts/Release/Kick inside WORKTREE.
set -eu

DRY=0
for a in "$@"; do
    case "$a" in
        --dry-run) DRY=1 ;;
        -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "build_arm.sh: unknown argument: $a" >&2; exit 2 ;;
    esac
done

WORKTREE="${WORKTREE:-$(git rev-parse --show-toplevel)}"
CPM_CACHE="${CPM_CACHE:-$HOME/.cache/kick-cpm}"
JOBS="${JOBS:-2}"
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-}"

if [ -z "$TOOLCHAIN_DIR" ] || [ ! -f "$TOOLCHAIN_DIR/armv7.cmake" ]; then
    TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$(mktemp -d)}"
    mkdir -p "$TOOLCHAIN_DIR"
    cat > "$TOOLCHAIN_DIR/armv7.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
EOF
fi
mkdir -p "$CPM_CACHE"

echo "build_arm.sh: worktree=$WORKTREE jobs=$JOBS cache=$CPM_CACHE toolchain=$TOOLCHAIN_DIR"
echo "build_arm.sh: the build takes about 4-5 minutes"
set -- docker run --rm -v "$WORKTREE":/workspace -v "$CPM_CACHE":/cache -v "$TOOLCHAIN_DIR":/tc:ro \
    -e PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig -e PKG_CONFIG_SYSROOT_DIR=/ \
    kick-armv7-builder bash -c "set -e
 cmake -S Kick -B build-arm-cpm -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/tc/armv7.cmake -DKICK_ENABLE_HARDWARE=ON -DKICK_TOUCH_SINGLE_TAP_LISTBOX=ON -DCPM_SOURCE_CACHE=/cache -DTIMESTAMP=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
 cmake --build build-arm-cpm --target Kick -j$JOBS"

if [ "$DRY" -eq 1 ]; then
    echo "build_arm.sh: dry run, would run:"
    echo "$@"
    exit 0
fi

( cd "$WORKTREE" && "$@" )
BIN="$WORKTREE/build-arm-cpm/apps/kick/Kick_artefacts/Release/Kick"
if [ ! -f "$BIN" ]; then
    echo "build_arm.sh: build finished but $BIN does not exist" >&2
    exit 1
fi
echo "build_arm.sh: OK $BIN ($(wc -c < "$BIN") bytes)"
