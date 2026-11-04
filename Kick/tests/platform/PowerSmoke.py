#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Power window / shutdown sequence smoke of the real app (isolated data root, no board devices, desktop PlatformServices: the
power off / reboot is only logged).

  python3 PowerSmoke.py <Kick app> <resource root>
"""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

app, resources = sys.argv[1:3]
with tempfile.TemporaryDirectory(prefix="kick-power-smoke-") as directory:
    root = Path(directory)
    data = root / "data"
    shots = root / "shots"
    data.mkdir()
    shots.mkdir()
    env = dict(os.environ, KICK_DATA_ROOT=str(data), KICK_RESOURCE_ROOT=resources, KICK_NO_AUDIO_DEVICE="1",
               KICK_NO_SERIAL_DEVICE="1", KICK_NO_HARDWARE="1", KICK_SMOKE_POWER="1", KICK_SMOKE_SHOTS_DIR=str(shots))
    expect = ["POWER window open=1", "POWER window cancel ok=1", "POWER step 1 unsaved question open=1", "POWER step 1 cancel ok=1",
              "POWER step 2 autosave+restart ok=1", "[POWER] power: restart requested",
              "Desktop reboot requested (nothing is done)", "PASS: power"]
    with (root / "app.log").open("w+") as log:
        child = subprocess.Popen([app, "--smoke-test"], env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True, cwd=resources)
        try:
            code = child.wait(timeout=60)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
            raise SystemExit("FAIL: app exceeded the watchdog")
        log.seek(0)
        output = log.read()
    print(output)
    if code != 0:
        raise SystemExit(f"FAIL: app exited {code}")
    if "FAIL" in output or "JUCE Assertion" in output or "Leaked objects" in output:
        raise SystemExit("FAIL: the app reported a failure")
    for line in expect:
        if line not in output:
            raise SystemExit(f"FAIL: missing '{line}'")
    if not (shots / "Dialog_Power.png").is_file():
        raise SystemExit("FAIL: no screenshot of the Power window")
    print("PASS: power smoke")
