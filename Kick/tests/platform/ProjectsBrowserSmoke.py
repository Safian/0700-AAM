#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Projects / file browser smoke runs of the real app (isolated data root, no board devices).

  python3 ProjectsBrowserSmoke.py <Kick app> <resource root> browser    sorted list without hidden files in the browser screenshot
  python3 ProjectsBrowserSmoke.py <Kick app> <resource root> projects   project operations (engine) + the load queue
"""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

app, resources, mode = sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix="kick-prj-smoke-") as directory:
    root = Path(directory)
    data = root / "data"
    shots = root / "shots"
    data.mkdir()
    shots.mkdir()
    env = dict(os.environ, KICK_DATA_ROOT=str(data), KICK_RESOURCE_ROOT=resources, KICK_NO_AUDIO_DEVICE="1",
               KICK_NO_SERIAL_DEVICE="1", KICK_NO_HARDWARE="1")
    if mode == "browser":
        fixture = root / "fixture"
        (fixture / "Sub").mkdir(parents=True)
        (fixture / "Beat").mkdir()
        (fixture / "Beat" / "Beat.kickPrj").write_text("<ROOT/>")
        for name in ("Kick 10.wav", "Kick 2.wav", ".DS_Store", "._junk.wav", "notes.txt"):
            (fixture / name).write_text("x")
        env.update(KICK_SMOKE_SCREENS="MenuFileBrowser,RecentProject", KICK_SMOKE_SHOTS_DIR=str(shots),
                   KICK_SMOKE_BROWSE=str(fixture), KICK_SMOKE_BROWSE_EXPECT="Beat,Sub,Kick 2.wav,Kick 10.wav,notes.txt")
        expect = ["BROWSER rows: Beat,Sub,Kick 2.wav,Kick 10.wav,notes.txt"]
    elif mode == "projects":
        env.update(KICK_SMOKE_PROJECTS="queue", KICK_SMOKE_SCREENS=",".join(["HomeOptions"] * 12), KICK_SMOKE_SHOTS_DIR=str(shots))
        expect = ["PROJECTS: missing project refused", "PROJECTS: save a copy ok", "PROJECTS: save as ok", "PROJECTS: don't save ok",
                  "PROJECTS: demo ok", "PROJECTS: load queue ok", "PASS: project load queue"]
    else:
        raise SystemExit("unknown mode")
    with (root / "app.log").open("w+") as log:
        child = subprocess.Popen([app, "--smoke-test"], env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True, cwd=resources)
        try:
            code = child.wait(timeout=100)
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
    print(f"PASS: {mode} smoke")
