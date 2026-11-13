# Documentation index

Repo-wide rules: `AGENTS.md` (root). Scope: mobile only, Akai MPC support discontinued.
All documentation lives under `docs/`. Paths below are relative to `docs/`. Deleted documents stay in git history (`git log --diff-filter=D --name-only -- docs`).

## archive/akai-key61/ – ARCHIVÁLT, NEM FEJLESZTENDŐ (2026-11-13)
Az Akai MPC támogatás megszűnt (lásd root `AGENTS.md` SCOPE). A régi Key 61 dokumentáció történeti hivatkozásként itt van; ne indulj el rá. `hardware/key61/tools/` (szkriptek) a SynthyBoyVST MPC-build és egy host-teszt miatt maradt a helyén, lásd `hardware/key61/tools/README.md`.

## planning/ – open work
- `todo-master.md` – the TODO: only open items (defects, remaining units, human checks, owner decisions, later units).
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

A régi `hardware/key61/*.md`, `photos/` és `planning/grid-manual-audit.md` az `archive/akai-key61/` alá költözött (`git log --follow`).
