# BehringerKick – agent instructions

Local fork of `MusicTribeOpenSource/0700-AAM` (Behringer KICK, AGPL-3.0-or-later).
Goal: contribute to Kick upstream (iOS/iPad port, scalable UI, slide-out hardware controls) and ship
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
- Branch `feat/mobile-slideout-ui`: macOS `Kick` target builds and starts. Other targets, tests, hardware and iOS unverified.
- iOS simulator (Xcode 27.1 SDK): `Kick` target builds, but the app exits at launch with
  "UIScene life cycle is required" (log: `xcrun simctl spawn <dev> log show --predicate 'process == "Kick"'`).
  JUCE 7.0.12 uses the legacy single-window `UIApplicationDelegate`; fix needs a JUCE version with UIScene
  support (check JUCE 8 release notes; the multitouch patch is pinned to 7.0.12) or a custom scene delegate.
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
