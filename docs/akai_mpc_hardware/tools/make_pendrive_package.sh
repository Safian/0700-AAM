#!/bin/sh
# make_pendrive_package.sh: Build the bootable pendrive zip package for Akai MPC Key 61
#
# Generates:
#   dist/kick-mpc-key61-<version>.zip
#   dist/kick-mpc-key61-<version>.zip.sha256
#
# Directory layout inside zip:
#   tkgl_bootstrap_[ProjectData]/
#     VERSION
#     bin/
#       Kick
#     scripts/
#       tkgl_bootstrap
#       uninstall_kick.sh
#     assets/
#     templates/
#     demos/
#     logs/
#
# Usage:
#   make_pendrive_package.sh [options]
# Options:
#   --binary <path>         Path to Kick ARMv7 binary
#                           (default: build-arm-cpm/apps/kick/Kick_artefacts/Release/Kick)
#   --version <ver>         Package version string (default: auto from git)
#   --git-commit <sha>      Git commit hash (default: current HEAD)
#   --assets-dir <path>     Assets directory (default: Kick/assets)
#   --templates-dir <path>  Templates directory (default: Kick/templates)
#   --demos-dir <path>      Demos directory (default: Kick/demos)
#   --out-dir <path>        Output directory (default: dist)
#   --allow-dummy-binary    Allow non-ARM / dummy binary for testing
#   --dry-run               Validate and print actions without creating zip
#   -h, --help              Show this help message
set -eu

TOP_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

BINARY_PATH=""
VERSION=""
GIT_COMMIT=""
ASSETS_DIR="$TOP_DIR/Kick/assets"
TEMPLATES_DIR="$TOP_DIR/Kick/templates"
DEMOS_DIR="$TOP_DIR/Kick/demos"
OUT_DIR="$TOP_DIR/dist"
ALLOW_DUMMY_BINARY=0
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)
            BINARY_PATH="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --git-commit)
            GIT_COMMIT="$2"
            shift 2
            ;;
        --assets-dir)
            ASSETS_DIR="$2"
            shift 2
            ;;
        --templates-dir)
            TEMPLATES_DIR="$2"
            shift 2
            ;;
        --demos-dir)
            DEMOS_DIR="$2"
            shift 2
            ;;
        --out-dir)
            OUT_DIR="$2"
            shift 2
            ;;
        --allow-dummy-binary)
            ALLOW_DUMMY_BINARY=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            sed -n '2,27p' "$0"
            exit 0
            ;;
        *)
            echo "make_pendrive_package.sh: unknown option: $1" >&2
            exit 2
            ;;
    esac
done

# 1. Resolve binary path
if [ -z "$BINARY_PATH" ]; then
    DEFAULT_BIN="$TOP_DIR/build-arm-cpm/apps/kick/Kick_artefacts/Release/Kick"
    if [ -f "$DEFAULT_BIN" ]; then
        BINARY_PATH="$DEFAULT_BIN"
    elif [ "$ALLOW_DUMMY_BINARY" -eq 1 ]; then
        BINARY_PATH="/tmp/mock_kick_binary_$$"
        echo '#!/bin/sh' > "$BINARY_PATH"
        echo 'echo "Mock Kick binary operational"' >> "$BINARY_PATH"
        chmod +x "$BINARY_PATH"
    else
        echo "make_pendrive_package.sh: ERROR binary not found at default: $DEFAULT_BIN" >&2
        echo "Specify --binary <path> or use --allow-dummy-binary for testing." >&2
        exit 1
    fi
fi

if [ ! -f "$BINARY_PATH" ]; then
    echo "make_pendrive_package.sh: ERROR binary does not exist: $BINARY_PATH" >&2
    exit 1
fi

# 2. Check binary architecture (unless --allow-dummy-binary)
if [ "$ALLOW_DUMMY_BINARY" -ne 1 ]; then
    if command -v od >/dev/null 2>&1; then
        HDR="$(od -An -tx1 -N20 "$BINARY_PATH" | tr -s ' \n' ' ')"
        # shellcheck disable=SC2086
        set -- $HDR
        if [ "${1:-}" != "7f" ] || [ "${2:-}" != "45" ] || [ "${3:-}" != "4c" ] || [ "${4:-}" != "46" ] || [ "${5:-}" != "01" ] || [ "${19:-}" != "28" ] || [ "${20:-}" != "00" ]; then
            echo "make_pendrive_package.sh: ERROR $BINARY_PATH is not a 32-bit ARM ELF binary (header: $HDR)" >&2
            echo "If packaging a test or mock binary, pass --allow-dummy-binary." >&2
            exit 1
        fi
    fi
fi

# 3. Version resolution
if [ -z "$VERSION" ]; then
    if command -v git >/dev/null 2>&1 && git rev-parse --git-dir >/dev/null 2>&1; then
        VERSION="$(git describe --tags --always --dirty 2>/dev/null || echo "0.1.0-alpha")"
    else
        VERSION="0.1.0-alpha"
    fi
fi

if [ -z "$GIT_COMMIT" ]; then
    if command -v git >/dev/null 2>&1 && git rev-parse --git-dir >/dev/null 2>&1; then
        GIT_COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")"
    else
        GIT_COMMIT="unknown"
    fi
fi

calc_sha256() {
    target_file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$target_file" 2>/dev/null | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$target_file" 2>/dev/null | awk '{print $1}'
    else
        echo "unknown"
    fi
}

BIN_SHA="$(calc_sha256 "$BINARY_PATH")"
BUILD_DATE="$(date -u '+%Y-%m-%d %H:%M:%SZ' 2>/dev/null || date)"

ZIP_BASENAME="kick-mpc-key61-${VERSION}.zip"
ZIP_PATH="$OUT_DIR/$ZIP_BASENAME"
SHA_PATH="$ZIP_PATH.sha256"

echo "=== Packaging Behringer Kick for Akai MPC Key 61 ==="
echo "Version:         $VERSION ($GIT_COMMIT)"
echo "Build Date:      $BUILD_DATE"
echo "Binary:          $BINARY_PATH (SHA256: $BIN_SHA)"
echo "Assets Dir:      $ASSETS_DIR"
echo "Templates Dir:   $TEMPLATES_DIR"
echo "Demos Dir:       $DEMOS_DIR"
echo "Output Zip:      $ZIP_PATH"

if [ "$DRY_RUN" -eq 1 ]; then
    echo "[DRY RUN] Finished checks. Exiting without creating zip."
    exit 0
fi

# 4. Prepare Staging Directory
STAGE_DIR="$(mktemp -d "/tmp/kick_package_stage_XXXXXX")"
cleanup() {
    rm -rf "$STAGE_DIR"
    case "$BINARY_PATH" in
        /tmp/mock_kick_binary_*) rm -f "$BINARY_PATH" ;;
    esac
}
trap cleanup EXIT INT TERM

ROOT_DIR="$STAGE_DIR/tkgl_bootstrap_[ProjectData]"
mkdir -p "$ROOT_DIR/bin" "$ROOT_DIR/scripts" "$ROOT_DIR/logs" "$ROOT_DIR/conf"

# Copy binary
cp -f "$BINARY_PATH" "$ROOT_DIR/bin/Kick"
chmod 755 "$ROOT_DIR/bin/Kick"

# Copy bootstrap and uninstall scripts
TOOLS_DIR="$TOP_DIR/docs/akai_mpc_hardware/tools"
if [ ! -f "$TOOLS_DIR/tkgl_bootstrap" ]; then
    echo "make_pendrive_package.sh: ERROR $TOOLS_DIR/tkgl_bootstrap missing!" >&2
    exit 1
fi
cp -f "$TOOLS_DIR/tkgl_bootstrap" "$ROOT_DIR/scripts/tkgl_bootstrap"
chmod 755 "$ROOT_DIR/scripts/tkgl_bootstrap"

if [ -f "$TOOLS_DIR/uninstall_kick.sh" ]; then
    cp -f "$TOOLS_DIR/uninstall_kick.sh" "$ROOT_DIR/scripts/uninstall_kick.sh"
    chmod 755 "$ROOT_DIR/scripts/uninstall_kick.sh"
fi

# Copy resources (assets, templates, demos)
if [ -d "$ASSETS_DIR" ]; then
    mkdir -p "$ROOT_DIR/assets"
    cp -R "$ASSETS_DIR/"* "$ROOT_DIR/assets/"
fi

if [ -d "$TEMPLATES_DIR" ]; then
    mkdir -p "$ROOT_DIR/templates"
    cp -R "$TEMPLATES_DIR/"* "$ROOT_DIR/templates/"
fi

if [ -d "$DEMOS_DIR" ]; then
    mkdir -p "$ROOT_DIR/demos"
    cp -R "$DEMOS_DIR/"* "$ROOT_DIR/demos/"
fi

# Ensure logs dir has placeholder
touch "$ROOT_DIR/logs/.gitkeep"

# Write VERSION file
cat > "$ROOT_DIR/VERSION" <<EOF
VERSION=$VERSION
GIT_COMMIT=$GIT_COMMIT
BUILD_DATE=$BUILD_DATE
KICK_SHA256=$BIN_SHA
EOF

# 5. Create Zip Archive
mkdir -p "$OUT_DIR"
rm -f "$ZIP_PATH" "$SHA_PATH"

echo "Compressing package into $ZIP_PATH..."
(
    cd "$STAGE_DIR"
    # Ensure correct permissions in zip
    chmod -R u+w,go+r "tkgl_bootstrap_[ProjectData]"
    chmod 755 "tkgl_bootstrap_[ProjectData]/bin/Kick"
    chmod 755 "tkgl_bootstrap_[ProjectData]/scripts/tkgl_bootstrap"
    [ -f "tkgl_bootstrap_[ProjectData]/scripts/uninstall_kick.sh" ] && chmod 755 "tkgl_bootstrap_[ProjectData]/scripts/uninstall_kick.sh"
    zip -r -q "$ZIP_PATH" "tkgl_bootstrap_[ProjectData]"
)

# 6. Generate SHA256 checksum file
(
    cd "$OUT_DIR"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$ZIP_BASENAME" > "$ZIP_BASENAME.sha256"
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$ZIP_BASENAME" > "$ZIP_BASENAME.sha256"
    fi
)

ZIP_SIZE="$(wc -c < "$ZIP_PATH" | tr -d ' ')"
echo "=== Package Built Successfully ==="
echo "Archive:  $ZIP_PATH ($ZIP_SIZE bytes)"
echo "Checksum: $SHA_PATH"
cat "$SHA_PATH"
