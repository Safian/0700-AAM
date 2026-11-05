# Documentation index

Repo-wide rules: `AGENTS.md` (root). Hardware and on-device debugging rules: `Kick/apps/kick/AGENTS.md`.
All documentation lives under `docs/`. Paths below are relative to `docs/`.

## hardware/key61/ – Akai MPC Key 61
- `pendrive-install.md` – pendrive-os telepítési, frissítési, visszaállítási és használati útmutató.
- `README.md` – knowledge base entry point (official manuals, sources, checksums).
- `hw-behaviour.md` – measured hardware behaviour (ports, channels, LEDs, SysEx, boot). Append every new fact.
- `usb-and-phone.md` – Controller Mode and phone/tablet use (manual and web research, unverified on hardware).
- `led-colours.md`, `touch-strip-modes.md`, `doc-vs-kick.md` – LEDs, touch strip, manual vs Kick.
- `build-recipe.md` – ARMv7 build recipe. `hw-test-checklist.md`, `hw-test-report.md`, `todo.md`, `morning-decisions.md` – hardware test log, TODO and decisions.
- `walkthrough-plan.md`, `walkthrough/` – manual walkthrough work packages and claims registers (`wp-b.raw.json`, `wp-d.md`, `wp-g.md`). `local-model-pilot.md` – the local-model pilot for them.
- `tools/` – `tkgl_bootstrap`, `make_pendrive_package.sh`, `uninstall_kick.sh`, `test_tkgl_bootstrap.sh`, `build_arm.sh`, `deploy_mpc.sh`, `mpcsnoop.c` (script names unchanged, they are referenced by name from code and the root `README.md`).
- `captures/` – recorded raw MIDI (capture dates are in the first line of each file: `# captured: YYYY-MM-DD`).
- `photos/` – device photo and panel schematics. `manuals/` – git-ignored PDFs (see `README.md` there).

## planning/ – plans and TODO
- `todo-master.md` – master TODO. `morning-notes.md` – morning summary/notes.
- `plans/` – plans (pendrive image, piano CLAP, plugin ecosystem, screen audit, FX), `plans/research/`, `plans/screens/`, `plans/assets/`.

## upstream/ – upstream and fork work
- `upstream-triage.md` – triage of the fork commits into upstream PRs and fork-only tickets. Its per-ticket file lists are verified against the git history, so the old paths inside it are historical and are intentionally not rewritten.
- `fork-push-advice.md` – tanácsok a fork-feltöltéshez és az upstream-munkához (mi van pusholva, mi nem; push-szabályok; a triázs állapota).
- `tools/check_upstream_triage.py` – verifies `upstream-triage.md` against git.

## Other
- `manual/` – user and developer manual (`USER_MANUAL.md`, with sidebars). `images/` – screenshots used by the docs and the root `README.md`.

## Old path -> new path
Search for an old name here. (Per-file history is kept: `git log --follow <new path>`.)

| Old (under `docs/`) | New (under `docs/`) |
|---|---|
| `akai_mpc_hardware/README.md` | `hardware/key61/README.md` |
| `akai_mpc_hardware/PENDRIVE_INSTALL.md` | `hardware/key61/pendrive-install.md` |
| `akai_mpc_hardware/KEY61_HW_BEHAVIOUR.md` | `hardware/key61/hw-behaviour.md` |
| `akai_mpc_hardware/KEY61_USB_AND_PHONE.md` | `hardware/key61/usb-and-phone.md` |
| `akai_mpc_hardware/KEY61_LED_COLOURS.md` | `hardware/key61/led-colours.md` |
| `akai_mpc_hardware/KEY61_TOUCH_STRIP_MODES.md` | `hardware/key61/touch-strip-modes.md` |
| `akai_mpc_hardware/KEY61_DOC_VS_KICK.md` | `hardware/key61/doc-vs-kick.md` |
| `akai_mpc_hardware/LOCAL_MODEL_PILOT.md` | `hardware/key61/local-model-pilot.md` |
| `akai_mpc_hardware/WALKTHROUGH_PLAN.md` | `hardware/key61/walkthrough-plan.md` |
| `akai_mpc_hardware/walkthrough/WP-B.raw.json` | `hardware/key61/walkthrough/wp-b.raw.json` |
| `akai_mpc_hardware/walkthrough/WP-D.md` | `hardware/key61/walkthrough/wp-d.md` |
| `akai_mpc_hardware/walkthrough/WP-G.md` | `hardware/key61/walkthrough/wp-g.md` |
| `akai_mpc_hardware/captures/key61_octave_leds_20261104.txt` | `hardware/key61/captures/octave-leds.txt (captured 2026-11-04)` |
| `akai_mpc_hardware/captures/key61_stock_mpc_startup_20261102.txt` | `hardware/key61/captures/stock-mpc-startup.txt (captured 2026-11-02)` |
| `akai_mpc_hardware/captures/key61_sweep_kick_first_20261102.txt` | `hardware/key61/captures/sweep-kick-first.txt (captured 2026-11-02)` |
| `akai_mpc_hardware/tools/*` | `hardware/key61/tools/* (names unchanged)` |
| `akai_mpc_hardware/manuals/` | `hardware/key61/manuals/` |
| `akai_mpc_key61/akai_mpc_key61_top_photo.jpg` | `hardware/key61/photos/top-photo.jpg` |
| `akai_mpc_key61/rear_panel_audio_usb_schematic.png` | `hardware/key61/photos/rear-panel-audio-usb-schematic.png` |
| `akai_mpc_key61/rear_panel_midi_cv_schematic.png` | `hardware/key61/photos/rear-panel-midi-cv-schematic.png` |
| `akai_mpc_key61/top_panel_left_schematic.png` | `hardware/key61/photos/top-panel-left-schematic.png` |
| `akai_mpc_key61/top_panel_right_schematic.png` | `hardware/key61/photos/top-panel-right-schematic.png` |
| `AKAI_MPC_KEY61_BUILD_RECIPE.md` | `hardware/key61/build-recipe.md` |
| `AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` | `hardware/key61/hw-test-checklist.md` |
| `AKAI_MPC_KEY61_HW_TEST_REPORT.md` | `hardware/key61/hw-test-report.md` |
| `AKAI_MPC_KEY61_TODO.md` | `hardware/key61/todo.md` |
| `AKAI_MPC_KEY61_MORNING_DECISIONS.md` | `hardware/key61/morning-decisions.md` |
| `TODO_MASTER.md` | `planning/todo-master.md` |
| `MORNING.md` | `planning/morning-notes.md` |
| `plans/**` | `planning/plans/** (names unchanged)` |
| `UPSTREAM_TRIAGE.md` | `upstream/upstream-triage.md` |
| `FORK_PUSH_ADVICE.md` | `upstream/fork-push-advice.md` |
| `tools/check_upstream_triage.py` | `upstream/tools/check_upstream_triage.py` |
