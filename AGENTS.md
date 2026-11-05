# BehringerKick – agent instructions

Local fork of `MusicTribeOpenSource/0700-AAM` (Behringer KICK, AGPL-3.0-or-later).
Goal: contribute to Kick upstream (iOS/iPad port, scalable full-screen UI with external MIDI controller connectivity) and ship
a **Proper** build that is Kick plus a skin only. Everything except the skin should be upstreamable.

## Layout
- `Kick/` – JUCE 7 application (`apps/`, `modules/`, `assets/`). Entry: `apps/kick`, simulator: `apps/kickem`.
- `AudIO/` – audio engine library. `ComProtocol/` – hardware packet protocol (`PktMgr`). `Linux/` – Buildroot image.
- UI is a fixed 1024x600 (`Kick/modules/kick_ui_controls/source/kick_layout.h`). Hardware input is abstracted as
  `CPktMgr::HWMsg_t` via `contracts::DclSignals` – software controls (pads/knobs/buttons) plug in there.
- Skins: `Theme::loadTheme(name)` + `Kick/assets/<name>/`. The Proper skin must live in its own `assets/Proper/`
  (never edit `assets/Default/`), selected by a CMake option.

## Rules
- Upstream first: one PR = one change, branched from clean `main`. Do not base upstream PRs on the local
  platform-glue commit (it is cherry-picked from upstream PR #13 and may diverge from upstream's fix for issue #9).
- Platform code behind `#if JUCE_IOS` / `JUCE_MAC`; Linux/hardware behaviour must not change.
- Keep the hardware 1024x600 behaviour the default; scaling/mobile layouts are opt-in.
- Mobile app (iOS/iPad): the Kick UI looks the same as on the Akai: full screen only, no hardware-lookalike chassis, drawers
  or on-screen hardware controls. Full operation needs an external MIDI controller connected; the mobile app is the Kick port
  to which external MIDI is attached.
- Trademarks: "Behringer" and "KICK" names/logos are not licensed. Proper builds use their own name and logo.
- No proprietary SDKs (ads, analytics) in this tree – AGPL.
- Keep existing license headers. Do not edit `build*/`, `.3rdPartySourceCache/` (both git-excluded locally).
- Use the smallest verified change; run the build target you touched.

## Build (macOS)
Tools: Homebrew `cmake` (4.x) and `ninja`; Xcode 27. JUCE 7.0.12 is cloned in `.3rdPartySourceCache/JUCE-7.0.12`.
```
export PATH=/opt/homebrew/bin:$PATH
MACOSX_DEPLOYMENT_TARGET=12.0 cmake -S Kick -B build -G Ninja \
  -DKICK_JUCE_PATH=$PWD/.3rdPartySourceCache/JUCE-7.0.12 -DKICK_SIMULATOR=ON -DTIMESTAMP=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
MACOSX_DEPLOYMENT_TARGET=12.0 ninja -C build Kick
```
`MACOSX_DEPLOYMENT_TARGET=12.0` is required (JUCE 7.0.12 uses APIs obsoleted in the macOS 15+ SDK otherwise).
Smoke test (no audio/serial/hardware):
```
KICK_DATA_ROOT=<tmpdir> KICK_RESOURCE_ROOT=$PWD/Kick KICK_NO_AUDIO_DEVICE=1 KICK_NO_SERIAL_DEVICE=1 \
KICK_NO_HARDWARE=1 KICK_SMOKE_SNAPSHOT=<tmpdir>/s.png build/Kick.app/Contents/MacOS/Kick --smoke-test
```
iOS: configure (with `MACOSX_DEPLOYMENT_TARGET=12.0` in the env, for the host tool juceaide) with `-G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator`; the Apple Development
team ID is `8R7QAX93QJ`. Simulator results are not evidence for audio timing – verify on a device.

## Known state
- Mobile port (iOS/iPad): Full-screen DAW UI with external MIDI controller connectivity (CoreMIDI). Hardware-mimicking chassis and slide-out drawer removed (owner decision 2026-11-04): the Kick mobile port is a full-screen mobile app to which external MIDI controllers are connected.
- macOS `Kick` target builds and the smoke test starts the full-screen UI (verified 2026-11-04, no chassis).
- iOS: a UIScene patch for JUCE 7.0.12 is in the tree (`Kick/patches/juce_ios_uiscene_7.0.12.patch`, commit `2ec647d7`). Launch in the iOS simulator and on a device NOT re-verified after the chassis/drawer removal.
- Missing from the release: factory samples, Myriad Pro font, `juce_serialport`.

## Git
Commit only files you changed, by name (no `git add -A`). Put a report in the commit message: what changed,
what was not touched, which gate ran and its result, open questions. Start with `WIP:` if a gate failed.

## Working rules for agents (orchestrator vs workers)
- The orchestrating session only does SMALL work (git status, reads, short edits, planning). Everything long
  — builds (`ninja`, `cmake --build`, `make`), test runs, headless smoke runs (`xvfb-run`), merge+build+test+push
  sequences, wait loops — is delegated to a worker (Agent tool) or run with `run_in_background: true`.
- Never wait in the foreground (`until`/`while`/`sleep >= 10`): background jobs notify on completion.
  A foreground command is cancelled when the user sends a message, so long foreground commands also get killed.
- Enforced by `.claude/hooks/block-foreground-long-cmds.py` (PreToolUse on Bash, wired in `.claude/settings.json`):
  such foreground commands are blocked with a message. Keep the hook and this rule in sync.
- Hand one worker a whole multi-step task (e.g. merge, build, test, push) instead of splitting it across the orchestrator.

## Build and test (Linux, native x86 — cloud container)
Run `bash .claude/hooks/setup-linux-build.sh` once per fresh container (idempotent; log `/tmp/kick-setup.log`): it installs the
headers (ALSA, X11/XInput2, freetype, fontconfig, GL, curl, JACK, xvfb), clones JUCE 7.0.12 into `.3rdPartySourceCache/` and
configures `build-linux/` (all git-excluded). Run it in the background.
```
ninja -C build-linux Kick        # full app incl. the hardware bridge (apps/kick/Main.cpp); run in the background
```
Host tests (no JUCE/ALSA), from `Kick/tests/platform`: `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t XTests.cpp && /tmp/t`
(`PadColourMapTests` takes `../../assets/Default/theme.xml` as its argument). They are also registered in that directory's `CMakeLists.txt`.
Headless screenshots (smoke test): `KICK_SMOKE_SHOTS_DIR=<dir> KICK_SMOKE_SCREENS="HomeGridView,MenuPadMixer" KICK_DATA_ROOT=<tmp>
KICK_RESOURCE_ROOT=$PWD/Kick KICK_NO_AUDIO_DEVICE=1 KICK_NO_SERIAL_DEVICE=1 KICK_NO_HARDWARE=1 xvfb-run -a build-linux/apps/kick/Kick_artefacts/Release/Kick --smoke-test`.
Per-change compile check without a full build: take the TU command from `build-linux/compile_commands.json`, swap the path
prefix to your worktree, drop `-o`/`-MD`, add `-fsyntax-only`. Syntax checks must cover every TU the change reaches (e.g. the `audio_program.cpp` unity TU, `kick_ui.cpp`, `Main.cpp`), not only `audio_engine.cpp`; `verify-green.sh` is the final judge.
Worker template: `.claude/agents/kick-worker.md`.

## Akai MPC Key 61 (hardware)
Hardware, ARM build/deploy and on-device debugging rules live in `Kick/apps/kick/AGENTS.md` (loaded when you work there).
Push only to the `fork` remote (`HEAD:safian/main`), never `origin`. Documentation index: `docs/README.md`.
