#!/bin/bash
# Full verification of the CURRENT HEAD: native Linux build + all host tests.
# On success writes the HEAD sha to .git/kick-verified-head (read by the push gate).
# Long-running: ALWAYS run it in the background (run_in_background) or from a worker.
cd "$(git rev-parse --show-toplevel)" || exit 1
HEAD_SHA=$(git rev-parse HEAD)
rm -f .git/kick-verified-head
echo "== BUILD"; ninja -C build-linux Kick 2>&1 | grep -E "error|FAILED|Linking CXX executable|ninja:" | head -20 | cut -c1-260
ninja -C build-linux Kick >/dev/null 2>&1 || { echo "== DONE fail=1 (build)"; exit 1; }
echo "== TESTS"; fail=0
T=$(mktemp -d)
cd Kick/tests/platform || exit 1
for src in *Tests.cpp; do
  case "$src" in SerialTests.cpp|KeyboardDetectorTests.cpp|ClapInstrumentProcessorTests.cpp|AudioTests.cpp) continue;; esac
  n=${src%.cpp}
  g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o "$T/$n" "$src" 2>&1 | head -3
  if [ "$n" = "PadColourMapTests" ]; then "$T/$n" ../../assets/Default/theme.xml >/dev/null; else "$T/$n" >/dev/null; fi
  rc=$?; echo "$n rc=$rc"; [ $rc -ne 0 ] && fail=1
done
rm -rf "$T"
cd - >/dev/null
if [ $fail -eq 0 ]; then echo "$HEAD_SHA" > .git/kick-verified-head; echo "== DONE fail=0 (verified $HEAD_SHA)"; else echo "== DONE fail=1"; exit 1; fi
