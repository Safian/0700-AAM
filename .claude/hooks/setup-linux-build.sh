#!/bin/bash
# SessionStart hook (async): make a fresh Linux cloud container able to build and test Kick natively.
# Idempotent: skips whatever is already present. Never runs the full build. Log: /tmp/kick-setup.log
[ "$(uname -s)" = "Linux" ] || exit 0
ROOT="${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel 2>/dev/null)}"
[ -n "$ROOT" ] && cd "$ROOT" || exit 0
LOG=/tmp/kick-setup.log
{
  echo "== $(date) kick linux setup in $ROOT"
  # 1) system headers (ALSA, X11 + XInput2, freetype, fontconfig, GL, curl, JACK)
  need=0
  for f in /usr/include/alsa/asoundlib.h /usr/include/X11/extensions/XInput2.h /usr/include/freetype2 \
           /usr/include/fontconfig/fontconfig.h /usr/include/jack/jack.h /usr/include/GL/gl.h; do
    [ -e "$f" ] || need=1
  done
  if [ $need -eq 1 ] && [ "$(id -u)" = "0" ] && command -v apt-get >/dev/null; then
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
      libxcursor-dev libxext-dev libxi-dev libxrender-dev libxcomposite-dev libxfixes-dev libfreetype-dev \
      libfontconfig1-dev libgl1-mesa-dev libcurl4-openssl-dev libjack-jackd2-dev pkg-config xvfb
  else
    echo "system headers: ok or not root (need=$need)"
  fi
  # 2) JUCE 7.0.12 (git-excluded cache, see AGENTS.md)
  if [ ! -f .3rdPartySourceCache/JUCE-7.0.12/CMakeLists.txt ]; then
    mkdir -p .3rdPartySourceCache
    git clone -q --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git .3rdPartySourceCache/JUCE-7.0.12
  fi
  # 3) keep generated dirs out of git locally
  for p in build-linux/ .3rdPartySourceCache/ .claude/worktrees/; do
    grep -qxF "$p" .git/info/exclude 2>/dev/null || echo "$p" >> .git/info/exclude
  done
  # 4) configure (not build)
  if [ ! -f build-linux/build.ninja ] && [ -f .3rdPartySourceCache/JUCE-7.0.12/CMakeLists.txt ]; then
    cmake -S Kick -B build-linux -G Ninja -DKICK_JUCE_PATH="$PWD/.3rdPartySourceCache/JUCE-7.0.12" \
      -DKICK_SIMULATOR=ON -DTIMESTAMP=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  fi
  echo "== done $(date)"
} >> "$LOG" 2>&1
exit 0
