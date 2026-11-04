#!/bin/sh
# test_tkgl_bootstrap.sh: Automated offline verification test suite for Kick MPC pendrive packaging & bootstrap
#
# Tests:
#  1. Syntax validation (sh -n) and shellcheck on all scripts
#  2. make_pendrive_package.sh creation of zip, directory tree, VERSION file, and SHA256 checksum
#  3. Scenario 1: Fresh install onto empty /data (binary, assets, templates, demos, VERSION, logs)
#  4. Scenario 2: Boot with identical VERSION (no flash write, logs match)
#  5. Scenario 3: Update with differing VERSION (Kick.prev backup, Kick updated, Kick.version updated)
#  6. Scenario 4: Corrupt copy / SHA256 mismatch (install aborted, old binary preserved, Kick.new cleaned up)
#  7. Scenario 5: Missing binary and empty /data (exits 0 to allow stock MPC fallback)
#  8. Scenario 6: Preservation of user projects (/data/projects) and sounds across installs
#  9. Scenario 7: USB UNINSTALL trigger (/media/PenDrive/UNINSTALL removes Kick and exits 0)
# 10. Scenario 8: Standalone uninstall_kick.sh execution and --all cleanup
# 11. Scenario 9: Install cut by a power loss after the binary swap (Kick.prev keeps the real previous version)
# 12. Scenario 10: No sha256 tool on the system / VERSION without KICK_SHA256 (install refused, old Kick kept)
# 13. Scenario 11: ROLLBACK trigger (USB file and /data/Kick.rollback), refused rollbacks, rejected package not reinstalled
set -eu

TOP_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
TOOLS_DIR="$TOP_DIR/docs/akai_mpc_hardware/tools"

PASS_COUNT=0
FAIL_COUNT=0

pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  [PASS] $*"
}

fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "  [FAIL] $*" >&2
}

assert_file_exists() {
    if [ -f "$1" ]; then
        pass "File exists: $1"
    else
        fail "File missing: $1"
    fi
}

assert_dir_exists() {
    if [ -d "$1" ]; then
        pass "Directory exists: $1"
    else
        fail "Directory missing: $1"
    fi
}

assert_file_executable() {
    if [ -x "$1" ]; then
        pass "File is executable: $1"
    else
        fail "File not executable: $1"
    fi
}

assert_file_not_exists() {
    if [ ! -e "$1" ]; then
        pass "Path does not exist as expected: $1"
    else
        fail "Path unexpectedly exists: $1"
    fi
}

echo "=========================================================="
echo " Kick Akai MPC Pendrive Packaging & Bootstrap Test Suite "
echo "=========================================================="

# -----------------------------------------------------------------------------
# Test 1: Syntax & Linting Checks
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 1: Syntax & Linting (sh -n & shellcheck) ---"

SCRIPTS="
$TOOLS_DIR/tkgl_bootstrap
$TOOLS_DIR/uninstall_kick.sh
$TOOLS_DIR/make_pendrive_package.sh
$TOOLS_DIR/test_tkgl_bootstrap.sh
"

for s in $SCRIPTS; do
    if sh -n "$s"; then
        pass "sh -n syntax OK: $(basename "$s")"
    else
        fail "sh -n syntax error in: $s"
    fi

    if command -v shellcheck >/dev/null 2>&1; then
        if shellcheck -s sh "$s"; then
            pass "shellcheck OK: $(basename "$s")"
        else
            fail "shellcheck warning/error in: $s"
        fi
    fi
done

# -----------------------------------------------------------------------------
# Test 2: make_pendrive_package.sh Execution & Layout Verification
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 2: Package Generation (make_pendrive_package.sh) ---"

TEST_TMP="$(mktemp -d "/tmp/kick_test_env_XXXXXX")"
# shellcheck disable=SC2329
cleanup() {
    rm -rf "$TEST_TMP"
}
trap cleanup EXIT INT TERM

DIST_TEST="$TEST_TMP/dist"
MOCK_BIN="$TEST_TMP/Kick_mock_v1"
cat > "$MOCK_BIN" <<'EOF'
#!/bin/sh
echo "Mock Kick binary version 1.0"
EOF
chmod +x "$MOCK_BIN"

# shellcheck disable=SC2034
PKG_OUT="$("$TOOLS_DIR/make_pendrive_package.sh" \
    --binary "$MOCK_BIN" \
    --version "0.1.0-test" \
    --git-commit "test1234" \
    --out-dir "$DIST_TEST" \
    --allow-dummy-binary)"

ZIP_FILE="$DIST_TEST/kick-mpc-key61-0.1.0-test.zip"
SHA_FILE="$ZIP_FILE.sha256"

assert_file_exists "$ZIP_FILE"
assert_file_exists "$SHA_FILE"

# Verify SHA256 checksum file format and validity
(
    cd "$DIST_TEST"
    if command -v sha256sum >/dev/null 2>&1; then
        if sha256sum -c "$(basename "$SHA_FILE")" >/dev/null 2>&1; then
            pass "sha256sum verification succeeded for package"
        else
            fail "sha256sum verification failed for package"
        fi
    elif command -v shasum >/dev/null 2>&1; then
        if shasum -a 256 -c "$(basename "$SHA_FILE")" >/dev/null 2>&1; then
            pass "shasum -a 256 verification succeeded for package"
        else
            fail "shasum -a 256 verification failed for package"
        fi
    fi
)

# Unpack and verify zip layout
UNPACK_DIR="$TEST_TMP/unpacked"
mkdir -p "$UNPACK_DIR"
unzip -q "$ZIP_FILE" -d "$UNPACK_DIR"

TKGL_DIR="$UNPACK_DIR/tkgl_bootstrap_[ProjectData]"
assert_file_exists "$TKGL_DIR/VERSION"
assert_file_executable "$TKGL_DIR/bin/Kick"
assert_file_executable "$TKGL_DIR/scripts/tkgl_bootstrap"
assert_file_executable "$TKGL_DIR/scripts/uninstall_kick.sh"

if [ -d "$TKGL_DIR/assets" ]; then
    pass "Assets folder included in package"
else
    fail "Assets folder missing from package"
fi

if [ -d "$TKGL_DIR/templates" ]; then
    pass "Templates folder included in package"
else
    fail "Templates folder missing from package"
fi

if [ -d "$TKGL_DIR/demos" ]; then
    pass "Demos folder included in package"
else
    fail "Demos folder missing from package"
fi

# Verify VERSION file contents
if grep -q "VERSION=0.1.0-test" "$TKGL_DIR/VERSION" && grep -q "KICK_SHA256=" "$TKGL_DIR/VERSION"; then
    pass "VERSION file contains correct version and KICK_SHA256"
else
    fail "VERSION file content invalid"
fi

# -----------------------------------------------------------------------------
# Test 3: Scenario 1 - Fresh Install onto Empty /data
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 3: Scenario 1 - Fresh Install on Empty /data ---"

MOCK_DATA="$TEST_TMP/mock_data"
MOCK_USB="$TEST_TMP/mock_usb/PenDrive"
mkdir -p "$MOCK_DATA" "$MOCK_USB"

# Copy unpacked package into mock USB
cp -R "$TKGL_DIR" "$MOCK_USB/"
USB_TKGL="$MOCK_USB/tkgl_bootstrap_[ProjectData]"

# Create mock tkgl_path
cat > "$USB_TKGL/scripts/tkgl_path" <<EOF
DEVICE="/dev/sdb2"
MOUNT_POINT="$MOCK_USB"
TKGL_ROOT="$USB_TKGL"
TKGL_SCRIPT="$USB_TKGL/scripts"
TKGL_BIN="$USB_TKGL/bin"
TKGL_LOGS="$USB_TKGL/logs"
TKGL_CONF="$USB_TKGL/conf"
TKGL_LIB="$USB_TKGL/lib"
EOF

# Run bootstrap with KICK_BOOT_DRY_RUN=1 to test install without exec
KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot1.stdout" 2>&1

assert_file_exists "$MOCK_DATA/Kick"
assert_file_executable "$MOCK_DATA/Kick"
assert_file_exists "$MOCK_DATA/Kick.version"
assert_file_exists "$MOCK_DATA/kick-boot.log"
assert_file_exists "$USB_TKGL/logs/kick-boot.log"

if cmp -s "$USB_TKGL/VERSION" "$MOCK_DATA/Kick.version"; then
    pass "Local Kick.version exactly matches package VERSION"
else
    fail "Kick.version does not match package VERSION"
fi

if [ -d "$MOCK_DATA/assets" ] && [ -d "$MOCK_DATA/templates" ] && [ -d "$MOCK_DATA/demos" ]; then
    pass "Resources (assets, templates, demos) installed onto /data"
else
    fail "Resources not copied onto /data"
fi

if grep -q "Installation/update finished successfully" "$MOCK_DATA/kick-boot.log"; then
    pass "Boot log recorded successful installation"
else
    fail "Boot log missing successful installation record"
fi

# -----------------------------------------------------------------------------
# Test 4: Scenario 2 - Boot with Identical VERSION (No Flash Write)
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 4: Scenario 2 - Boot with Identical VERSION ---"

# Set a sentinel marker or check modification time
touch -r "$MOCK_DATA/Kick" "$TEST_TMP/kick_mtime_ref"

KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot2.stdout" 2>&1

if grep -q "Flash write skipped" "$TEST_TMP/boot2.stdout"; then
    pass "Bootstrap skipped flash write when version matches"
else
    fail "Bootstrap did not skip flash write for identical version"
fi

# Check that Kick binary was not modified
if [ "$MOCK_DATA/Kick" -ot "$TEST_TMP/kick_mtime_ref" ] || [ "$MOCK_DATA/Kick" -nt "$TEST_TMP/kick_mtime_ref" ]; then
    fail "Binary modification time unexpectedly changed on identical version boot"
else
    pass "Binary was left completely untouched on identical version boot"
fi

# -----------------------------------------------------------------------------
# Test 5: Scenario 3 - Update with Differing VERSION
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 5: Scenario 3 - Version Difference Update ---"

MOCK_BIN_V2="$TEST_TMP/Kick_mock_v2"
cat > "$MOCK_BIN_V2" <<'EOF'
#!/bin/sh
echo "Mock Kick binary version 2.0"
EOF
chmod +x "$MOCK_BIN_V2"

cp -f "$MOCK_BIN_V2" "$USB_TKGL/bin/Kick"
V2_SHA="$("$TOOLS_DIR/make_pendrive_package.sh" --help >/dev/null && (sha256sum "$MOCK_BIN_V2" 2>/dev/null || shasum -a 256 "$MOCK_BIN_V2") | awk '{print $1}')"

cat > "$USB_TKGL/VERSION" <<EOF
VERSION=0.2.0-test
GIT_COMMIT=v2commit
BUILD_DATE=2026-11-04
KICK_SHA256=$V2_SHA
EOF

KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot3.stdout" 2>&1

assert_file_exists "$MOCK_DATA/Kick.prev"
if grep -q "version 1.0" "$MOCK_DATA/Kick.prev"; then
    pass "Kick.prev contains original version 1.0 binary"
else
    fail "Kick.prev does not contain original version 1.0 binary"
fi

if grep -q "version 2.0" "$MOCK_DATA/Kick"; then
    pass "Kick now contains updated version 2.0 binary"
else
    fail "Kick does not contain updated version 2.0 binary"
fi

if grep -q "VERSION=0.2.0-test" "$MOCK_DATA/Kick.version"; then
    pass "Kick.version updated to 0.2.0-test"
else
    fail "Kick.version was not updated"
fi

# -----------------------------------------------------------------------------
# Test 6: Scenario 4 - Corrupt Binary / SHA256 Mismatch
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 6: Scenario 4 - Corrupt Copy / SHA256 Mismatch ---"

# Deliberately put bad binary or bad SHA
echo "CORRUPTED BYTES" > "$USB_TKGL/bin/Kick"
cat > "$USB_TKGL/VERSION" <<EOF
VERSION=0.3.0-corrupted
GIT_COMMIT=corrupt123
BUILD_DATE=2026-11-04
KICK_SHA256=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
EOF

KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot4.stdout" 2>&1

assert_file_not_exists "$MOCK_DATA/Kick.new"

if grep -q "version 2.0" "$MOCK_DATA/Kick"; then
    pass "Existing valid version 2.0 binary preserved on corrupted update"
else
    fail "Existing binary corrupted or overwritten by bad update"
fi

if grep -q "VERSION=0.2.0-test" "$MOCK_DATA/Kick.version"; then
    pass "Kick.version retained valid version 0.2.0-test"
else
    fail "Kick.version was incorrectly changed by corrupt update"
fi

if grep -q "SHA256 mismatch" "$TEST_TMP/boot4.stdout"; then
    pass "Integrity mismatch detected and logged properly"
else
    fail "Integrity check did not detect corrupted binary"
fi

# -----------------------------------------------------------------------------
# Test 7: Scenario 5 - Missing Binary on USB and Empty /data -> Exit 0
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 7: Scenario 5 - Missing Binary & Empty /data (Exit 0) ---"

EMPTY_DATA="$TEST_TMP/mock_empty_data"
mkdir -p "$EMPTY_DATA"
rm -f "$USB_TKGL/bin/Kick"

STATUS=0
KICK_DATA_DIR="$EMPTY_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot5.stdout" 2>&1 || STATUS=$?

if [ "$STATUS" -eq 0 ]; then
    pass "Bootstrap exited 0 cleanly when no runnable binary exists (allows stock MPC fallback)"
else
    fail "Bootstrap returned non-zero code ($STATUS) on missing binary"
fi

if grep -q "Exiting 0 to let stock MPC start" "$TEST_TMP/boot5.stdout"; then
    pass "Fallback to stock MPC was explicitly logged"
else
    fail "Fallback message missing in log"
fi

# -----------------------------------------------------------------------------
# Test 8: Scenario 6 - User Projects & Custom Sounds Protection
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 8: Scenario 6 - Preservation of User Projects & Sounds ---"

# Restore valid binary
cp -f "$MOCK_BIN_V2" "$USB_TKGL/bin/Kick"
cat > "$USB_TKGL/VERSION" <<EOF
VERSION=0.4.0-user-check
GIT_COMMIT=v4commit
BUILD_DATE=2026-11-04
KICK_SHA256=$V2_SHA
EOF

# Create user project and sample on mock /data
mkdir -p "$MOCK_DATA/projects/MyEpicTrack" "$MOCK_DATA/sounds" "$MOCK_DATA/prefs"
echo "<Project XML Data>" > "$MOCK_DATA/projects/MyEpicTrack/MyEpicTrack.kickPrj"
echo "RIFF USER AUDIO SAMPLE" > "$MOCK_DATA/sounds/MyUserSample.wav"
echo "<Preferences XML>" > "$MOCK_DATA/prefs/preferences.settingsprj"

KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot6.stdout" 2>&1

if [ -f "$MOCK_DATA/projects/MyEpicTrack/MyEpicTrack.kickPrj" ] && grep -q "Project XML Data" "$MOCK_DATA/projects/MyEpicTrack/MyEpicTrack.kickPrj"; then
    pass "User project (/data/projects) completely preserved"
else
    fail "User project was deleted or modified"
fi

if [ -f "$MOCK_DATA/sounds/MyUserSample.wav" ] && grep -q "USER AUDIO SAMPLE" "$MOCK_DATA/sounds/MyUserSample.wav"; then
    pass "User sample (/data/sounds) completely preserved"
else
    fail "User sample was deleted or modified"
fi

if [ -f "$MOCK_DATA/prefs/preferences.settingsprj" ] && grep -q "Preferences XML" "$MOCK_DATA/prefs/preferences.settingsprj"; then
    pass "User preferences (/data/prefs) completely preserved"
else
    fail "User preferences was deleted or modified"
fi

# -----------------------------------------------------------------------------
# Test 9: Scenario 7 - USB UNINSTALL Trigger
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 9: Scenario 7 - USB UNINSTALL Trigger ---"

touch "$MOCK_USB/UNINSTALL"

STATUS=0
KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" > "$TEST_TMP/boot7.stdout" 2>&1 || STATUS=$?

if [ "$STATUS" -eq 0 ]; then
    pass "Bootstrap returned 0 on UNINSTALL trigger (allowing stock MPC to boot)"
else
    fail "Bootstrap returned non-zero ($STATUS) on UNINSTALL"
fi

assert_file_not_exists "$MOCK_DATA/Kick"
assert_file_not_exists "$MOCK_DATA/Kick.version"
assert_file_not_exists "$MOCK_DATA/Kick.new"
assert_file_not_exists "$MOCK_DATA/Kick.prev"
assert_file_not_exists "$MOCK_USB/UNINSTALL"
assert_file_exists "$MOCK_USB/UNINSTALL.done"

# Verify user project still intact!
if [ -f "$MOCK_DATA/projects/MyEpicTrack/MyEpicTrack.kickPrj" ]; then
    pass "User project survived UNINSTALL"
else
    fail "User project was deleted during UNINSTALL"
fi

# -----------------------------------------------------------------------------
# Test 10: Scenario 8 - Standalone uninstall_kick.sh Script
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 10: Scenario 8 - Standalone uninstall_kick.sh ---"

# Reinstall mock Kick
rm -f "$MOCK_USB/UNINSTALL.done"
KICK_DATA_DIR="$MOCK_DATA" KICK_BOOT_DRY_RUN=1 sh "$USB_TKGL/scripts/tkgl_bootstrap" >/dev/null 2>&1
assert_file_exists "$MOCK_DATA/Kick"
assert_dir_exists "$MOCK_DATA/assets"

# Run uninstaller with --all
"$TOOLS_DIR/uninstall_kick.sh" --data-dir "$MOCK_DATA" --all --yes > "$TEST_TMP/uninstall.stdout" 2>&1

assert_file_not_exists "$MOCK_DATA/Kick"
assert_file_not_exists "$MOCK_DATA/Kick.version"
assert_file_not_exists "$MOCK_DATA/assets"
assert_file_not_exists "$MOCK_DATA/templates"
assert_file_not_exists "$MOCK_DATA/demos"

# Verify user project still intact!
if [ -f "$MOCK_DATA/projects/MyEpicTrack/MyEpicTrack.kickPrj" ]; then
    pass "User project survived standalone uninstall_kick.sh --all"
else
    fail "User project was deleted by standalone uninstall_kick.sh"
fi


# -----------------------------------------------------------------------------
# Helpers for the scenarios 9-11: a package folder with a mock binary and a matching VERSION
# -----------------------------------------------------------------------------
sha_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

# expect <ok text> <failure text> <command...>: pass when the command succeeds.
expect() {
    ex_ok="$1"; ex_bad="$2"; shift 2
    if "$@"; then pass "$ex_ok"; else fail "$ex_bad"; fi
}

# make_pkg <dir> <label> [nosha]: tkgl layout in <dir> with bin/Kick printing "label" and VERSION=<label>.
make_pkg() {
    mp_dir="$1"; mp_label="$2"
    mkdir -p "$mp_dir/scripts" "$mp_dir/bin" "$mp_dir/logs"
    cp -f "$TOOLS_DIR/tkgl_bootstrap" "$mp_dir/scripts/tkgl_bootstrap"
    printf '#!/bin/sh\necho "Mock Kick %s"\n' "$mp_label" > "$mp_dir/bin/Kick"
    chmod +x "$mp_dir/bin/Kick"
    {
        echo "VERSION=$mp_label"
        echo "GIT_COMMIT=c$mp_label"
        echo "BUILD_DATE=2026-11-04"
        if [ "${3:-}" != "nosha" ]; then echo "KICK_SHA256=$(sha_of "$mp_dir/bin/Kick")"; fi
    } > "$mp_dir/VERSION"
    cat > "$mp_dir/scripts/tkgl_path" <<EOP
DEVICE="/dev/sdb2"
MOUNT_POINT="$(dirname "$mp_dir")"
TKGL_ROOT="$mp_dir"
TKGL_SCRIPT="$mp_dir/scripts"
TKGL_BIN="$mp_dir/bin"
TKGL_LOGS="$mp_dir/logs"
TKGL_CONF="$mp_dir/conf"
TKGL_LIB="$mp_dir/lib"
EOP
}

run_boot() {  # run_boot <pkgdir> <datadir> <outfile> [extra env assignments via env]
    rb_status=0
    KICK_DATA_DIR="$2" KICK_BOOT_DRY_RUN=1 sh "$1/scripts/tkgl_bootstrap" > "$3" 2>&1 || rb_status=$?
    return 0
}

# -----------------------------------------------------------------------------
# Test 11: Scenario 9 - Install cut after the binary swap (power loss)
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 11: Scenario 9 - Interrupted update keeps Kick.prev ---"

S9="$TEST_TMP/s9"; S9_DATA="$S9/data"; mkdir -p "$S9_DATA"
make_pkg "$S9/usb1/pkg" "1.0.0"
run_boot "$S9/usb1/pkg" "$S9_DATA" "$TEST_TMP/s9a.out"
expect "S9: v1 installed" "S9: v1 not installed" grep -q "Mock Kick 1.0.0" "$S9_DATA/Kick"
expect "S9: no install marker left after a complete install" "S9: marker left behind" [ ! -e "$S9_DATA/Kick.installing" ]

make_pkg "$S9/usb2/pkg" "2.0.0"
rb_status=0
KICK_DATA_DIR="$S9_DATA" KICK_BOOT_DRY_RUN=1 KICK_BOOT_TEST_ABORT_AFTER=replace sh "$S9/usb2/pkg/scripts/tkgl_bootstrap" > "$TEST_TMP/s9b.out" 2>&1 || rb_status=$?
expect "S9: simulated power cut after the swap (exit 99)" "S9: expected exit 99, got $rb_status" [ "$rb_status" -eq 99 ]
expect "S9: binary is already v2 when the power is cut" "S9: swap did not happen" grep -q "Mock Kick 2.0.0" "$S9_DATA/Kick"
expect "S9: Kick.version still says v1 (written last)" "S9: Kick.version changed too early" grep -q "VERSION=1.0.0" "$S9_DATA/Kick.version"
expect "S9: Kick.prev holds v1" "S9: Kick.prev lost v1" grep -q "Mock Kick 1.0.0" "$S9_DATA/Kick.prev"
expect "S9: install marker present" "S9: install marker missing" [ -f "$S9_DATA/Kick.installing" ]

run_boot "$S9/usb2/pkg" "$S9_DATA" "$TEST_TMP/s9c.out"
expect "S9: retry recognises the interrupted install" "S9: interrupted install not recognised" grep -q "Interrupted installation detected" "$TEST_TMP/s9c.out"
expect "S9: Kick.prev still holds the real previous version (v1)" "S9: Kick.prev was overwritten by the half-installed binary" grep -q "Mock Kick 1.0.0" "$S9_DATA/Kick.prev"
expect "S9: Kick.version.prev still pairs with Kick.prev" "S9: Kick.version.prev not paired" grep -q "VERSION=1.0.0" "$S9_DATA/Kick.version.prev"
expect "S9: retry finished: Kick.version = v2" "S9: retry did not finish" grep -q "VERSION=2.0.0" "$S9_DATA/Kick.version"
expect "S9: marker removed after the retry" "S9: marker still present" [ ! -e "$S9_DATA/Kick.installing" ]

# A cut BEFORE the swap leaves the old binary: the retry backs it up normally.
make_pkg "$S9/usb3/pkg" "3.0.0"
run_boot "$S9/usb3/pkg" "$S9_DATA" "$TEST_TMP/s9d.out"
expect "S9: a normal update 2 -> 3 backs up v2" "S9: normal update did not back up v2" grep -q "Mock Kick 2.0.0" "$S9_DATA/Kick.prev"
if ls "$S9_DATA"/*.tmp >/dev/null 2>&1; then fail "S9: temp files left in /data"; else pass "S9: no temp files left in /data"; fi

# -----------------------------------------------------------------------------
# Test 12: Scenario 10 - no sha256 tool / VERSION without KICK_SHA256
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 12: Scenario 10 - Unverifiable package is refused ---"

S10="$TEST_TMP/s10"; S10_DATA="$S10/data"; mkdir -p "$S10_DATA"
make_pkg "$S10/usb1/pkg" "1.0.0"
run_boot "$S10/usb1/pkg" "$S10_DATA" "$TEST_TMP/s10a.out"

# (a) VERSION without KICK_SHA256: refuse, keep v1
make_pkg "$S10/usb2/pkg" "2.0.0" nosha
run_boot "$S10/usb2/pkg" "$S10_DATA" "$TEST_TMP/s10b.out"
expect "S10: missing KICK_SHA256 is reported" "S10: missing KICK_SHA256 not reported" grep -q "no KICK_SHA256" "$TEST_TMP/s10b.out"
expect "S10: installed v1 kept" "S10: installed Kick was replaced by an unverifiable package" grep -q "Mock Kick 1.0.0" "$S10_DATA/Kick"
expect "S10: Kick.version unchanged" "S10: Kick.version changed" grep -q "VERSION=1.0.0" "$S10_DATA/Kick.version"
assert_file_not_exists "$S10_DATA/Kick.new"

# (b) no sha tool on the system: only the tools the script needs, no sha256sum / shasum
NOSHA_BIN="$TEST_TMP/nosha_bin"; mkdir -p "$NOSHA_BIN"
for t in cp mv rm mkdir date cmp head grep cut tr awk chmod sync touch dirname cat printf ls sed; do
    tp="$(command -v "$t" 2>/dev/null || true)"
    case "$tp" in /*) ln -sf "$tp" "$NOSHA_BIN/$t" ;; esac
done
make_pkg "$S10/usb3/pkg" "3.0.0"
rb_status=0
PATH="$NOSHA_BIN" KICK_DATA_DIR="$S10_DATA" KICK_BOOT_DRY_RUN=1 /bin/sh "$S10/usb3/pkg/scripts/tkgl_bootstrap" > "$TEST_TMP/s10c.out" 2>&1 || rb_status=$?
expect "S10: missing sha tool is reported" "S10: missing sha tool not reported" grep -q "no sha256sum/shasum available" "$TEST_TMP/s10c.out"
expect "S10: installed v1 kept without a sha tool" "S10: Kick replaced without verification" grep -q "Mock Kick 1.0.0" "$S10_DATA/Kick"

# (c) fresh /data and an unverifiable package: nothing installed, exit 0 (stock MPC)
S10_EMPTY="$S10/empty"; mkdir -p "$S10_EMPTY"
rb_status=0
KICK_DATA_DIR="$S10_EMPTY" KICK_BOOT_DRY_RUN=1 sh "$S10/usb2/pkg/scripts/tkgl_bootstrap" > "$TEST_TMP/s10d.out" 2>&1 || rb_status=$?
expect "S10: fresh /data + unverifiable package exits 0 (stock MPC)" "S10: non-zero exit $rb_status" [ "$rb_status" -eq 0 ]
assert_file_not_exists "$S10_EMPTY/Kick"

# -----------------------------------------------------------------------------
# Test 13: Scenario 11 - ROLLBACK
# -----------------------------------------------------------------------------
echo ""
echo "--- Test 13: Scenario 11 - ROLLBACK trigger ---"

S11="$TEST_TMP/s11"; S11_DATA="$S11/data"; mkdir -p "$S11_DATA"
make_pkg "$S11/usb1/pkg" "1.0.0"; run_boot "$S11/usb1/pkg" "$S11_DATA" "$TEST_TMP/s11a.out"

# Rollback without a previous version: refused, nothing changes, the boot continues.
touch "$S11/usb1/ROLLBACK"
run_boot "$S11/usb1/pkg" "$S11_DATA" "$TEST_TMP/s11b.out"
expect "S11: rollback without Kick.prev refused" "S11: rollback without Kick.prev not refused" grep -q "ROLLBACK refused: no previous version" "$TEST_TMP/s11b.out"
expect "S11: Kick unchanged after the refused rollback" "S11: Kick changed" grep -q "Mock Kick 1.0.0" "$S11_DATA/Kick"
assert_file_exists "$S11/usb1/ROLLBACK.done"
assert_file_not_exists "$S11/usb1/ROLLBACK"

# Update to v2 from a second pendrive, then roll back from the pendrive that holds v2.
make_pkg "$S11/usb2/pkg" "2.0.0"; run_boot "$S11/usb2/pkg" "$S11_DATA" "$TEST_TMP/s11c.out"
expect "S11: v2 installed (prev = v1)" "S11: v2 not installed" grep -q "Mock Kick 2.0.0" "$S11_DATA/Kick"
touch "$S11/usb2/ROLLBACK"
run_boot "$S11/usb2/pkg" "$S11_DATA" "$TEST_TMP/s11d.out"
expect "S11: rollback completed" "S11: rollback did not complete" grep -q "ROLLBACK complete" "$TEST_TMP/s11d.out"
expect "S11: Kick is v1 again" "S11: Kick is not v1 after the rollback" grep -q "Mock Kick 1.0.0" "$S11_DATA/Kick"
expect "S11: Kick.version is v1 again" "S11: Kick.version not rolled back" grep -q "VERSION=1.0.0" "$S11_DATA/Kick.version"
assert_file_exists "$S11/usb2/ROLLBACK.done"
assert_file_exists "$S11_DATA/Kick.rejected"
expect "S11: same boot does not reinstall v2" "S11: v2 reinstalled in the rollback boot" grep -q "Package 2.0.0 was rolled back from" "$TEST_TMP/s11d.out"

# Next boot with the same pendrive: v2 is NOT installed again.
run_boot "$S11/usb2/pkg" "$S11_DATA" "$TEST_TMP/s11e.out"
expect "S11: rejected v2 is not reinstalled at the next boot" "S11: rollback undone by the next boot" grep -q "Mock Kick 1.0.0" "$S11_DATA/Kick"
expect "S11: rolled-back Kick is executable" "S11: rolled-back Kick not executable" [ -x "$S11_DATA/Kick" ]

# A different package (v3) clears the rejection and installs.
make_pkg "$S11/usb3/pkg" "3.0.0"; run_boot "$S11/usb3/pkg" "$S11_DATA" "$TEST_TMP/s11f.out"
expect "S11: a new package installs after a rollback" "S11: new package blocked" grep -q "Mock Kick 3.0.0" "$S11_DATA/Kick"
assert_file_not_exists "$S11_DATA/Kick.rejected"

# App-side trigger (/data/Kick.rollback): back to v1 (prev of the 1 -> 3 update), flag consumed.
touch "$S11_DATA/Kick.rollback"
run_boot "$S11/usb3/pkg" "$S11_DATA" "$TEST_TMP/s11g.out"
expect "S11: /data/Kick.rollback rolls back to the previous version" "S11: /data/Kick.rollback ignored" grep -q "Mock Kick 1.0.0" "$S11_DATA/Kick"
assert_file_not_exists "$S11_DATA/Kick.rollback"

# Corrupt Kick.prev: rollback refused, the running Kick stays.
make_pkg "$S11/usb4/pkg" "4.0.0"; run_boot "$S11/usb4/pkg" "$S11_DATA" "$TEST_TMP/s11h.out"
echo "TRUNCATED" > "$S11_DATA/Kick.prev"
touch "$S11/usb4/ROLLBACK"
run_boot "$S11/usb4/pkg" "$S11_DATA" "$TEST_TMP/s11i.out"
expect "S11: corrupt Kick.prev refused" "S11: corrupt Kick.prev accepted" grep -q "fails its SHA256 check" "$TEST_TMP/s11i.out"
expect "S11: running Kick kept after the refused rollback" "S11: Kick changed by a refused rollback" grep -q "Mock Kick 4.0.0" "$S11_DATA/Kick"

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
echo ""
echo "=========================================================="
echo " Test Suite Finished"
echo " Passed: $PASS_COUNT"
echo " Failed: $FAIL_COUNT"
echo "=========================================================="

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0
