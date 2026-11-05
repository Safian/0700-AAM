#!/bin/sh
# Install a Kick ARMv7 binary on the Akai MPC Key 61 and restart it the way the boot does.
#
# Usage: deploy_mpc.sh <binary> [--host 192.168.0.30] [--dry-run] [--keep-backups N]
#   --dry-run       check the binary, then only print the copy / remote commands
#   --keep-backups  after a successful install keep only the newest N /data/Kick.prev.* backups (default: delete nothing).
#                   /data/Kick.bak.* (the original binary) is never touched.
# The MPC starts Kick from the PenDrive bootstrap (tkgl_bootstrap) through the acvs service, so the restart is
# `systemctl restart acvs`. Lessons that are built in (all happened during development):
#  - never stop the app with kill $(ps | grep ...): the ssh command line itself matches the pattern and the session dies;
#  - never `nohup ... &` over ssh on BusyBox (it kills the process);
#  - replace the binary with mv (a rename), not by writing over the running file.
set -eu

BIN=""; HOST="192.168.0.30"; DRY=0; KEEP=""
while [ $# -gt 0 ]; do
    case "$1" in
        --host) HOST="$2"; shift 2 ;;
        --dry-run) DRY=1; shift ;;
        --keep-backups) KEEP="$2"; shift 2 ;;
        -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
        -*) echo "deploy_mpc.sh: unknown option: $1" >&2; exit 2 ;;
        *) BIN="$1"; shift ;;
    esac
done
[ -n "$BIN" ] || { echo "deploy_mpc.sh: no binary given (usage: deploy_mpc.sh <binary> [options])" >&2; exit 2; }

# a) the binary must exist and be a 32-bit ARM ELF (no `file` dependency)
[ -f "$BIN" ] || { echo "deploy_mpc.sh: FAIL binary not found: $BIN" >&2; exit 1; }
HDR="$(od -An -tx1 -N20 "$BIN" | tr -s ' \n' ' ')"
set -- $HDR
if [ "${1:-}" != "7f" ] || [ "${2:-}" != "45" ] || [ "${3:-}" != "4c" ] || [ "${4:-}" != "46" ] || [ "${5:-}" != "01" ] || [ "${19:-}" != "28" ] || [ "${20:-}" != "00" ]; then
    echo "deploy_mpc.sh: FAIL $BIN is not a 32-bit ARM ELF (header: $HDR)" >&2
    exit 1
fi
SIZE="$(wc -c < "$BIN" | tr -d ' ')"
echo "deploy_mpc.sh: OK  binary is a 32-bit ARM ELF, $SIZE bytes"

TS="$(date +%Y%m%d-%H%M)"
SSH="ssh -o ConnectTimeout=8 root@$HOST"
run() {   # run <description> <command...>
    desc="$1"; shift
    if [ "$DRY" -eq 1 ]; then echo "deploy_mpc.sh: DRY $desc: $*"; return 0; fi
    echo "deploy_mpc.sh: ...  $desc"
    "$@" < /dev/null || { echo "deploy_mpc.sh: FAIL $desc" >&2; exit 1; }
}

# b) copy next to the running binary
run "copy to /data/Kick.new" scp -q "$BIN" "root@$HOST:/data/Kick.new"
# c) backup of the running binary, then replace it with a rename
run "backup and replace" $SSH "cp /data/Kick /data/Kick.prev.$TS && mv /data/Kick.new /data/Kick && chmod +x /data/Kick"
# d) restart through systemd, in the background
run "restart acvs" $SSH "(systemctl restart acvs > /dev/null 2>&1 < /dev/null &); sleep 1"

if [ "$DRY" -eq 1 ]; then
    echo "deploy_mpc.sh: DRY wait 20 s, then check: ps | grep \"[K]ick\$\", grep -a -c operational /data/kick.log"
    [ -n "$KEEP" ] && echo "deploy_mpc.sh: DRY would keep the newest $KEEP /data/Kick.prev.* backups"
    exit 0
fi

# e) wait, then verify: the process must run and the log must show the bridge as operational
echo "deploy_mpc.sh: ...  waiting 20 s for Kick to start"
sleep 20
PROC="$($SSH 'ps | grep "[K]ick\$" | cut -c1-70' < /dev/null)" || { echo "deploy_mpc.sh: FAIL cannot read the MPC state" >&2; exit 1; }
if [ -z "$PROC" ]; then
    echo "deploy_mpc.sh: FAIL Kick is not running; last log lines:" >&2
    $SSH 'tail -n 5 /data/kick.log' < /dev/null >&2 || true
    exit 1
fi
OPER="$($SSH 'grep -a -c operational /data/kick.log' < /dev/null || true)"
if [ "${OPER:-0}" -lt 1 ] 2>/dev/null; then
    echo "deploy_mpc.sh: FAIL Kick runs ($PROC) but the log has no 'operational' line" >&2
    exit 1
fi
echo "deploy_mpc.sh: OK  Kick is running with the new binary: $PROC (backup: /data/Kick.prev.$TS)"

# f) optional pruning of old backups, only after a successful install
if [ -n "$KEEP" ]; then
    $SSH "n=\$(ls /data/Kick.prev.* 2>/dev/null | wc -l); if [ \$n -gt $KEEP ]; then ls -1 /data/Kick.prev.* | sort | head -n \$((n-$KEEP)) | xargs rm -f; fi; ls /data | grep -c '^Kick.prev.'" < /dev/null | sed 's/^/deploy_mpc.sh: backups kept: /'
fi
