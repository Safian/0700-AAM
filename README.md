# Safian Kick – mobile DAW

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL_3.0-blue.svg)](LICENSE)
[![Upstream: MusicTribeOpenSource/0700-AAM](https://img.shields.io/badge/Upstream-MusicTribeOpenSource%2F0700--AAM-orange.svg)](https://github.com/MusicTribeOpenSource/0700-AAM)

Independent, enhanced community fork of **Behringer KICK (`MusicTribeOpenSource/0700-AAM`)** by **[@Safian](https://github.com/Safian)**.

> **Scope (owner decision, 2026-11-13): Akai MPC support has been discontinued; do not develop for it.**
> Owner, verbatim: *"a kick-es akai fejlesztését el tudjuk engedni mostantól, az akaira fejlesztünk csak (a SynthyBoy-t)! max mobilra fogjuk folytatni. távolítsd el az akairól, és a telós változat meg annyit tudjon, hogy ha egy usb-c-vel csatlakoztatunk rá egy kidit akkor DAW-ként azt kiszolgálja."*
> Kick continues on mobile only (iOS/iPadOS; Android only if the repo gains a target). On Akai MPC hardware only SynthyBoy is developed (separate repo).
> **New requirement:** when something is attached over USB-C, the mobile app serves it as a DAW. Owner clarification (2026-11-13, Hungarian, verbatim): *"mondjuk egy akai-t kontrolermódban vagy egy midi zongorát, vagy egy akai mkII vagy hármat ... szóval minden szoftvermentes midi hardverből csinálna olyat mint az akai MPC hardverek - ez a cél kb"*. In short: any software-less USB-MIDI hardware (an Akai in controller mode, a MIDI piano, one or more Akai MK2 devices) becomes an MPC-like instrument/DAW with the phone running Kick. "kidit" was a typo.

## Features

- Native **CLAP plugin hosting** with lock-free event queues and per-note modulation (`clap_event_param_mod_t`).
- Controller-first, scalable full-screen UI (no skeuomorphic hardware chassis); external MIDI controllers via CoreMIDI.
- Multi-touch on-screen piano / keyboard.
- Upstream bug fixes: procedural metronome (PR #43), exact exponential ADSR (PR #44), uniform dark theme (PR #46), larger modal close touch targets (PR #48).

## Platforms

| Platform | Status |
|---|---|
| iOS / iPadOS | In progress (standalone touch DAW, CoreMIDI) |
| macOS (Apple Silicon / Intel) | Verified (development and smoke-test target) |
| Linux desktop | Builds (legacy hardware-bridge code paths are out of scope) |
| Akai MPC (Key 61, Live, One, X, Force) | **Discontinued, archived** – see `docs/archive/akai-key61/` |

## Building from Source (macOS)

Requirements: CMake 3.25+, Ninja, Xcode, JUCE 7.0.12.
```bash
export PATH=/opt/homebrew/bin:$PATH
MACOSX_DEPLOYMENT_TARGET=12.0 cmake -S Kick -B build -G Ninja \
  -DKICK_JUCE_PATH=$PWD/.3rdPartySourceCache/JUCE-7.0.12 \
  -DKICK_SIMULATOR=ON \
  -DTIMESTAMP=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

MACOSX_DEPLOYMENT_TARGET=12.0 ninja -C build Kick
```
Smoke test:
```bash
KICK_DATA_ROOT=/tmp/kick_data KICK_RESOURCE_ROOT=$PWD/Kick \
KICK_NO_AUDIO_DEVICE=1 KICK_NO_SERIAL_DEVICE=1 KICK_NO_HARDWARE=1 \
build/Kick.app/Contents/MacOS/Kick --smoke-test
```
Documentation index: [`docs/README.md`](docs/README.md).

## Legal, Trademarks & Licensing

- **Software License:** AGPL-3.0-or-later ([LICENSE](LICENSE)).
- **Independent Project:** not affiliated with, sponsored by, or endorsed by MUSIC Tribe Global Services Ltd., Behringer, or inMusic Brands, Inc.
- **Trademarks:** "Behringer" and "KICK" are trademarks of MUSIC Tribe Global Services Ltd.; "Akai" and "MPC" are registered trademarks of inMusic Brands, Inc. Names are used only to identify platforms for interoperability under fair use.

## Community & Upstream

- Upstream: [MusicTribeOpenSource/0700-AAM](https://github.com/MusicTribeOpenSource/0700-AAM); RFC [#26](https://github.com/MusicTribeOpenSource/0700-AAM/issues/26); PRs by [@Safian](https://github.com/MusicTribeOpenSource/0700-AAM/pulls/Safian).
