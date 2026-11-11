# Documentation index

Repo-wide rules: `AGENTS.md` (root). Hardware and on-device debugging rules: `Kick/apps/kick/AGENTS.md`.
All documentation lives under `docs/`. Paths below are relative to `docs/`. Deleted documents stay in git history (`git log --diff-filter=D --name-only -- docs`).

## hardware/key61/ – Akai MPC Key 61
- `pendrive-install.md` – pendrive-os telepítési, frissítési, visszaállítási és használati útmutató.
- `README.md` – knowledge base entry point (official manuals, sources, checksums).
- `hw-behaviour.md` – measured hardware behaviour (ports, channels, LEDs, SysEx, boot). Append every new fact.
- `usb-and-phone.md` – Controller Mode and phone/tablet use (manual and web research, unverified on hardware).
- `led-colours.md`, `touch-strip-modes.md`, `doc-vs-kick.md` – LEDs, touch strip, manual vs Kick.
- `build-recipe.md` – ARMv7 build recipe. `hw-test-checklist.md` – the hardware/listening checks not yet verified.
- `walkthrough-plan.md` – plan for the manual walkthrough (claims registers; WP-B/D/G registers were deleted as outdated). `local-model-pilot.md` – the local-model pilot for it.
- `tools/` – `tkgl_bootstrap`, `make_pendrive_package.sh`, `uninstall_kick.sh`, `test_tkgl_bootstrap.sh`, `build_arm.sh`, `build-armv7.sh`, `deploy_mpc.sh`, `mpcsnoop.c`, `make_piano_project.py`, PDF/pilot helpers (see `tools/README.md`; script names are referenced from code and the root `README.md`).
- `photos/` – device photo and panel schematics. `manuals/` – git-ignored PDFs (see `README.md` there).

## planning/ – open work
- `todo-master.md` – the TODO: only open items (defects, remaining units, human checks, owner decisions, later units).
- `grid-manual-audit.md` – point-by-point Grid View audit against Akai MPC v3.10, Serum 2, and Kick manuals (mobile & hardware parity).
- `screen-backlog.md` – open rows and round-end states of the screen reviews (S-C, S-D, S-E, S-F).
- `plans/02-piano-clap.md`, `03-plugin-ecosystem.md`, `05-fx.md` – piano CLAP, plugin ecosystem, FX plans (engine/plugin work runs in the SynthyBoyVST and KickEQ repos).
- `plans/research/` – `sample-sources.md`, `architecture-and-publishing.md` (piano plugin), `cloud-sources.md` (Splice / Google Drive terms).
- `plans/assets/piano-ui/` – UI mock-up for the piano plugin (its colours are already used by the on-screen keyboard).

## SYNTHYBOY-UI-REPORT.md
- `SYNTHYBOY-UI-REPORT.md` – the plug-in's own GUI in the plug-in panel (CLAP bitmap GUI extension client): what changed, precedence of a module over the built-in, Q-Link pages, measurements, what is verified and what is not, what the SynthyBoy side needs. Contract and usage: `../Kick/clap_host/README.md` ("Plug-in UI (bitmap GUI)").

## upstream/ – upstream and fork work
- `fork-push-advice.md` – push and upstream rules.

## Other
- `manual/` – user and developer manual (`USER_MANUAL.md`, with sidebars). `images/` – screenshots used by the docs and the root `README.md`.

## Old path -> new path
(Per-file history: `git log --follow <new path>`.)

| Old (under `docs/`) | New (under `docs/`) |
|---|---|
| `akai_mpc_hardware/*.md`, `KEY61_*.md`, `PENDRIVE_INSTALL.md` | `hardware/key61/` (lower-case names, e.g. `hw-behaviour.md`, `pendrive-install.md`) |
| `akai_mpc_hardware/tools/*`, `akai_mpc_hardware/manuals/` | `hardware/key61/tools/*`, `hardware/key61/manuals/` |
| `akai_mpc_key61/*` | `hardware/key61/photos/*` |
| `AKAI_MPC_KEY61_BUILD_RECIPE.md`, `..._HW_TEST_CHECKLIST.md` | `hardware/key61/build-recipe.md`, `hardware/key61/hw-test-checklist.md` |
| `TODO_MASTER.md` | `planning/todo-master.md` |
| `plans/**` | `planning/plans/**` |
| `FORK_PUSH_ADVICE.md` | `upstream/fork-push-advice.md` |
