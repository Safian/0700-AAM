#!/bin/sh
# uninstall_kick.sh: Restore Akai MPC Key 61 to factory state by removing Kick from internal flash.
#
# Usage: uninstall_kick.sh [options]
#   --data-dir <path>   Override target data directory (default: /data)
#   --all               Also remove Kick assets, templates, and demos
#   --purge-all         Also remove preferences and factory sounds (preserves projects)
#   --dry-run           Show what would be removed without deleting
#   -y, --yes           Skip confirmation prompt
#   -h, --help          Show this help message
#
# Note: The system boot hook (/usr/bin/az01-launch-MPC) is not modified.
# Once Kick is removed, the boot hook automatically launches the factory /usr/bin/MPC.
set -eu

DATA_DIR="/data"
REMOVE_ALL=0
PURGE_ALL=0
DRY_RUN=0
ASSUME_YES=0

while [ $# -gt 0 ]; do
    case "$1" in
        --data-dir)
            DATA_DIR="$2"
            shift 2
            ;;
        --all)
            REMOVE_ALL=1
            shift
            ;;
        --purge-all)
            REMOVE_ALL=1
            PURGE_ALL=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -y|--yes)
            ASSUME_YES=1
            shift
            ;;
        -h|--help)
            sed -n '2,13p' "$0"
            exit 0
            ;;
        *)
            echo "uninstall_kick.sh: unknown option: $1" >&2
            exit 2
            ;;
    esac
done

echo "=== Kick Uninstaller for Akai MPC ==="
echo "Target data directory: $DATA_DIR"
echo "Remove assets/templates/demos: $([ "$REMOVE_ALL" -eq 1 ] && echo "YES" || echo "NO")"
echo "Purge prefs/sounds: $([ "$PURGE_ALL" -eq 1 ] && echo "YES" || echo "NO")"
echo "User projects ($DATA_DIR/projects) are ALWAYS PRESERVED."

if [ "$ASSUME_YES" -ne 1 ] && [ "$DRY_RUN" -ne 1 ]; then
    printf "Are you sure you want to uninstall Kick from %s? [y/N]: " "$DATA_DIR"
    read -r answer
    case "$answer" in
        [yY]|[yY][eE][sS]) ;;
        *)
            echo "Aborted by user."
            exit 0
            ;;
    esac
fi

# 1. Stop running Kick process if any
echo "Checking for running Kick process..."
KICK_PIDS=""
if command -v pgrep >/dev/null 2>&1; then
    KICK_PIDS="$(pgrep -f "$DATA_DIR/Kick" 2>/dev/null || true)"
else
    # shellcheck disable=SC2009
    KICK_PIDS="$(ps | grep "[K]ick" | awk '{print $1}' 2>/dev/null || true)"
fi

if [ -n "$KICK_PIDS" ]; then
    echo "Stopping running Kick process (PID: $KICK_PIDS)..."
    if [ "$DRY_RUN" -eq 1 ]; then
        echo "[DRY RUN] Would kill -TERM $KICK_PIDS"
    else
        # shellcheck disable=SC2086
        kill -TERM $KICK_PIDS 2>/dev/null || true
        # Wait up to 5 seconds
        i=0
        while [ "$i" -lt 5 ]; do
            sleep 1
            # shellcheck disable=SC2086
            if ! kill -0 $KICK_PIDS 2>/dev/null; then
                break
            fi
            i=$((i + 1))
        done
        # shellcheck disable=SC2086
        if kill -0 $KICK_PIDS 2>/dev/null; then
            echo "Process did not terminate with SIGTERM, sending SIGKILL..."
            # shellcheck disable=SC2086
            kill -KILL $KICK_PIDS 2>/dev/null || true
        fi
    fi
fi

# 2. Files to remove
FILES_TO_REMOVE="
$DATA_DIR/Kick
$DATA_DIR/Kick.new
$DATA_DIR/Kick.prev
$DATA_DIR/Kick.version
$DATA_DIR/Kick.version.new
$DATA_DIR/Kick.version.prev
$DATA_DIR/Kick.installing
$DATA_DIR/Kick.rejected
$DATA_DIR/Kick.rollback
$DATA_DIR/kick_cmd
$DATA_DIR/kick_screen.ppm
$DATA_DIR/kick_probe.txt
$DATA_DIR/kick.crash
$DATA_DIR/kick-boot.log
$DATA_DIR/kick.log
$DATA_DIR/kick.log.old
"

for f in $FILES_TO_REMOVE; do
    if [ -e "$f" ]; then
        if [ "$DRY_RUN" -eq 1 ]; then
            echo "[DRY RUN] Would remove file: $f"
        else
            rm -f "$f"
            echo "Removed: $f"
        fi
    fi
done

# Backups pattern matching
for f in "$DATA_DIR"/Kick.prev.* "$DATA_DIR"/Kick.bak.*; do
    if [ -e "$f" ]; then
        if [ "$DRY_RUN" -eq 1 ]; then
            echo "[DRY RUN] Would remove backup: $f"
        else
            rm -f "$f"
            echo "Removed backup: $f"
        fi
    fi
done

# 3. Directories to remove if --all
if [ "$REMOVE_ALL" -eq 1 ]; then
    for dir in "$DATA_DIR/assets" "$DATA_DIR/templates" "$DATA_DIR/demos"; do
        if [ -d "$dir" ]; then
            if [ "$DRY_RUN" -eq 1 ]; then
                echo "[DRY RUN] Would remove directory: $dir"
            else
                rm -rf "$dir"
                echo "Removed directory: $dir"
            fi
        fi
    done
fi

# 4. Directories to remove if --purge-all
if [ "$PURGE_ALL" -eq 1 ]; then
    for dir in "$DATA_DIR/prefs" "$DATA_DIR/sounds"; do
        if [ -d "$dir" ]; then
            if [ "$DRY_RUN" -eq 1 ]; then
                echo "[DRY RUN] Would purge directory: $dir"
            else
                rm -rf "$dir"
                echo "Purged directory: $dir"
            fi
        fi
    done
fi

if [ "$DRY_RUN" -ne 1 ]; then
    sync 2>/dev/null || true
fi

echo ""
echo "=== Kick Uninstallation Completed Successfully ==="
echo "Internal flash ($DATA_DIR) has been cleaned."
echo "The bootstrap hook (/usr/bin/az01-launch-MPC) remains intact."
echo "When you restart the MPC (or run 'systemctl restart acvs'), the factory Akai MPC DAW will start."
