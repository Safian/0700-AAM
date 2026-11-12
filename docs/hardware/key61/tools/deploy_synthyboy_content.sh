#!/bin/sh
# Install the SynthyBoy factory presets (a content pack folder: <dir>/Bass, <dir>/Lead ... plus contentpack.json) on the Akai MPC Key 61.
#
# Usage: deploy_synthyboy_content.sh <pack-dir> [--host 192.168.0.30]
#   <pack-dir>  e.g. <SynthyBoyVST>/content/presets
# The plug-in reads <user dir>/packs/*/ : Kick sets SYNTHYBOY_USER_DIR=/data/synthyboy on Linux (the MPC starts the app without HOME), so the pack
# goes to /data/synthyboy/packs/factory. The old pack is replaced as a whole (a copy is kept as factory.prev until the next run).
set -eu
PACK=""; HOST="192.168.0.30"
while [ $# -gt 0 ]; do
    case "$1" in
        --host) HOST="$2"; shift 2 ;;
        -h|--help) sed -n '2,8p' "$0"; exit 0 ;;
        -*) echo "deploy_synthyboy_content.sh: unknown option: $1" >&2; exit 2 ;;
        *) PACK="$1"; shift ;;
    esac
done
[ -d "$PACK" ] || { echo "deploy_synthyboy_content.sh: not a directory: $PACK" >&2; exit 2; }
DEST=/data/synthyboy/packs
tar -C "$PACK" -cf - . | ssh "root@$HOST" "mkdir -p $DEST/factory.new && tar -C $DEST/factory.new -xf - && rm -rf $DEST/factory.prev && { [ -d $DEST/factory ] && mv $DEST/factory $DEST/factory.prev || true; } && mv $DEST/factory.new $DEST/factory && find $DEST/factory -name '*.sbpreset' | wc -l"
echo "deploy_synthyboy_content.sh: OK  pack installed in $DEST/factory on $HOST"
