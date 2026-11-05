# Kick app on the Akai MPC Key 61 (hardware work)

Loaded when working under `Kick/apps/kick/`. Repo-wide rules are in the root `AGENTS.md`; the doc index is `docs/README.md`.

## Remotes and pushing
Push only `git push fork HEAD:safian/main` (the owner's fork `Safian/0700-AAM`). Never push to `origin` (MusicTribe). Upstream PRs only when the owner says so.

## Where things are
- Hardware adapter and bridge: `hardware/adapters/akai_mpc_key61_adapter.h`, `kick_hardware_bridge.h`.
- Measured hardware behaviour: `docs/hardware/key61/hw-behaviour.md`; tools and scripts: `docs/hardware/key61/tools/`.
- Official v3.10 manuals (PDFs, git-ignored; sources and SHA-256 in `docs/hardware/key61/README.md`): documentation first, no hypotheses without evidence.
- Build and deploy: skill `kick-mpc-deploy` (user-level, outside the repo, in `~/.claude/skills/`) or `build_arm.sh` / `deploy_mpc.sh`.

## Debugging on the MPC Key 61 hardware (owner rule, 2026-11-02, all agents)
The owner must not be sent hopping around the device. Debug the way the 2026-11-02 Keygroup trace was done:
- **Drive the app yourself.** Everything that can be injected is injected through the `/data/kick_cmd` FIFO on the MPC
  (`echo "<cmd>" > /data/kick_cmd`): `btn <NAME>`, `touch X Y`, `touch_down/touch_move/touch_up`, `jog N`, `pad N V`, `key N V`,
  `strip V`, `rawmidi PORT hex...`, `snapshot /data/x.ppm` (convert the PPM to PNG and look at it). Touch space is the 1280x800
  framebuffer. Read `/data/kick.log` for what the app actually saw. Never ask the owner to tap or press something you can inject.
- **Trace the whole path in one go.** When something does not work, put temporary `[TAG]` log lines on every stage of the
  code path (input, router, program, engine, output) in ONE build, run the scenario, read the log, and see exactly where it stops.
  Do not guess one stage at a time. Remove the temporary lines before committing.
- **Rule the hardware out first.** `docs/hardware/key61/hw-behaviour.md` is the knowledge log of measured hardware
  behaviour (ports, channels, notes, LEDs, SysEx, boot state). What it marks as measured is not measured again; the recorded raw MIDI can be
  replayed with `rawmidi`. Every new measured fact is appended there (date, method, build).
- **Ask the human only for what only a human can perceive** (sound, LEDs, a physical control): one request, with the exact steps,
  and say what the result decides. Do not touch the screen while the owner is touching it; say so when you are about to drive it.
- **Deploy** with the `kick-mpc-deploy` skill or `docs/hardware/key61/tools/build_arm.sh` and `deploy_mpc.sh`. There is one MPC and one
  Docker build at a time (`-j2`). A deploy restarts Kick and drops the unsaved in-memory project: say so before you do it.
- Known traps of the X-less MPC: the top-level window is hidden (no `isShowing()`, no JUCE keyboard focus); touches are delivered by
  the bridge straight to `mouseDown/mouseDrag/mouseUp` (not through JUCE's dispatcher, so MouseListeners do not run); a double tap
  needs two taps within 450 ms; popups are separate windows that the snapshot and the injected touch may not reach.

# Mobile and desktop (non-hardware) builds

## Mobile App (iOS / iPadOS)
- **Architecture**: Full-screen scaled Kick DAW UI (1024x600 layout scaled to screen aspect ratio).
- **Control Model**: External MIDI controller connectivity (CoreMIDI, USB MIDI / Bluetooth MIDI). Full operational control is handled via connected external MIDI devices.
- **UI Policy**: No hardware-mimicking chassis, simulated front panel, or slide-out/peeking drawer. The mobile port is a pure full-screen mobile app identical to the standalone DAW engine on Akai, designed for tactile control through external MIDI hardware (owner decision 2026-11-04).

## Desktop (macOS)
- Native document window presenting the Kick DAW interface directly at 1024x600 without hardware chassis simulation.
