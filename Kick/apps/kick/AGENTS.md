# Kick app: hardware (Akai MPC) work is OUT OF SCOPE

**Scope: az Akai MPC támogatás megszűnt, ne fejlessz rá (owner, 2026-11-13).** Ez a fájl korábban az Akai MPC Key 61 hardveres munkájának szabályait tartalmazta; a Kick mostantól csak mobil. Az Akai-adapter (`hardware/adapters/akai_mpc_key61_adapter.h`), a `mpc_gui` / `mpc_piano` appok és a `kick_hardware_bridge.h` Akai-ágai legacy kód: ne bővítsd, ne tesztelj rajtuk hardveren, ne csatlakozz MPC-hez.

Remotes: push only `git push fork HEAD:safian/develop` (or `safian/main`), never `origin`. Upstream PRs only when the owner says so. Repo-wide rules: root `AGENTS.md`; doc index: `docs/README.md`. Archived Akai knowledge: `docs/archive/akai-key61/` (read-only history).
