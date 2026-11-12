# Behringer KICK (0700-AAM) — Layer 2 CLAP Host Architecture & MancerMap Schema

**Author:** Safian Szabolcs (@Safian) & Dream Maker 5 (@FYD5IVE)  
**License:** AGPL-3.0-or-later / MIT (CLAP ABI)  
**Status:** Layer 2 Staging Specification (Built on top of PR #27)

---

## 1. Overview & Two-Layer Strategy

To ensure clean upstream separation between real-time DSP audio engine internals and controller-first user-space workflows, the 0700-AAM CLAP synthesizer host is organized into two distinct layers:

```
+-----------------------------------------------------------------------+
| LAYER 2: Controller-First UI & Declarative Macro Schema (Safian)     |
| - MancerMap declarative JSON / .mancer hardware parameter profiles    |
| - 16 RGB Pad performance mappings, Q-Link / Macro labeling           |
| - DAW Grid View real-time step and live recording visual feedback    |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| LAYER 1: AudIO Real-Time Engine & ComProtocol Bridge (FYD5IVE / PR #27)|
| - AudIO/processors/ClapInstrumentProcessor (Zero-allocation RT loop)  |
| - 256-slot SPSC parameter ring buffer (m_paramEventQueue: AudiomParamEvent) |
| - Dynamic loading of native aarch64 .clap binaries from SD card       |
| - Non-RT message-thread hooks: getPluginParamCount, queryPluginParamInfo |
| - 4x CV/Gate modulation outputs + Hybrid analog VCF/VCA loop          |
+-----------------------------------------------------------------------+
```

---

## 2. Open Interop `.mancer.json` v0.1 Stub Schema

To ensure seamless Layer 1 / Layer 2 interoperability, Dream Maker 5 (@FYD5IVE) specified the Open Interop `.mancer.json` v0.1 stub schema located under `/mnt/sdcard/Plugins/CLAP/MancerMaps/`:

```json
{
  "schema_version": "0.1-open-interop",
  "architect": "Dream Maker 5 (@FYD5IVE)",
  "license_notice": "Open Layer-2 UI Interop Stub. Full MancerMaps Compiler & Alch-Loop CV Matrices Copyright (c) 2026 Dream Maker 5 (@FYD5IVE).",
  "plugin_id": "com.u-he.Zebra3",
  "audiom_matrix_bindings": {
    "bank_a_encoders": [
      { "slot": "X1", "encoder_idx": 0, "clap_param_id": 1001, "range": [0.0, 1.0] },
      { "slot": "Y1", "encoder_idx": 1, "clap_param_id": 1002, "range": [0.0, 1.0] },
      { "slot": "X2", "encoder_idx": 2, "clap_param_id": 1003, "range": [0.0, 1.0] },
      { "slot": "Y2", "encoder_idx": 3, "clap_param_id": 1004, "range": [0.0, 1.0] }
    ],
    "bank_b_encoders": [
      { "slot": "X3", "encoder_idx": 0, "clap_param_id": 1005, "range": [0.0, 1.0] },
      { "slot": "Y3", "encoder_idx": 1, "clap_param_id": 1006, "range": [0.0, 1.0] },
      { "slot": "X4", "encoder_idx": 2, "clap_param_id": 1007, "range": [0.0, 1.0] },
      { "slot": "Y4", "encoder_idx": 3, "clap_param_id": 1008, "range": [0.0, 1.0] }
    ],
    "expressive_bus": {
      "touch_strip_param_id": 2001,
      "poly_aftertouch_mod_id": 2002
    }
  }
}
```

The parser in `AudIO/processors/MancerMap.hpp` binds `bank_a_encoders` (slots X1..Y2) to physical macros 0..3, `bank_b_encoders` (slots X3..Y4) to macros 4..7, and routes `expressive_bus` (`touch_strip_param_id` and `poly_aftertouch_mod_id`) to the hardware ribbon and polyphonic keybed pressure buses.

---

## 3. Declarative Legacy `.mancer` / `.json` Profile Schema

Instead of hardcoding parameter IDs inside the C++ audio thread, `MancerMap` also supports backward-compatible user-space mapping files:

```json
{
  "plugin_id": "com.u-he.zebralette3",
  "name": "Polykeys 61 Performance Map",
  "author": "@Safian",
  "macros": [
    { "index": 0, "param_id": 1001, "name": "Cutoff", "min": 20.0, "max": 20000.0, "default": 1000.0 },
    { "index": 1, "param_id": 1002, "name": "Resonance", "min": 0.0, "max": 1.0, "default": 0.2 },
    { "index": 2, "param_id": 1003, "name": "Morph X", "min": -1.0, "max": 1.0, "default": 0.0 },
    { "index": 3, "param_id": 1004, "name": "Morph Y", "min": -1.0, "max": 1.0, "default": 0.0 }
  ],
  "ribbon": {
    "param_id": 1005,
    "bipolar": true
  },
  "poly_aftertouch": true
}
```

### Schema Properties

| Field | Type | Description |
|---|---|---|
| `plugin_id` | String | Target CLAP plugin unique ID (e.g. `com.u-he.zebralette3`, `org.surge-synth-team.surge-xt`) |
| `name` | String | Display title for the mapping profile |
| `author` | String | Author attribution |
| `macros` | Array | Array of macro encoder bindings (indexes `0..7` mapping to Bank A / Bank B hardware encoders) |
| `macros[].index` | Integer | Physical macro index (`0..3` = Q-Links 1-4 Bank A, `4..7` = Bank B) |
| `macros[].param_id` | Integer | CLAP `clap_id` parameter identifier |
| `macros[].name` | String | Human-readable parameter label displayed on screen / OLEDs |
| `macros[].min` | Float | Minimum parameter value |
| `macros[].max` | Float | Maximum parameter value |
| `macros[].default` | Float | Initial default value |
| `ribbon.param_id` | Integer | Parameter ID mapped to the capacitive touch strip |
| `ribbon.bipolar` | Boolean | Whether touch strip operates bipolar (-1.0 to +1.0) or unipolar (0.0 to 1.0) |
| `poly_aftertouch` | Boolean | Enables polyphonic key pressure routing to the CLAP synth voice |

---

## 3. Real-Time Engine Integration

`ClapInstrumentProcessor` integrates `MancerMap` without compromising real-time safety:
1. **Zero Allocations in RT Path:** `applyMancerMap()` and `loadMancerMapFromFile()` run on non-RT control threads, updating fixed-capacity `macroParamIds_` and `macroNames_` arrays.
2. **Dynamic Querying:** UI controls query `getMacroName(index)` to render dynamic OLED / HUD labels.
3. **Graceful Fallback:** If no `.mancer` file exists for a plugin, `bindMacroParameters()` falls back to standard sequential CLAP parameter enumeration.

---

## 4. Verification & Testing

Unit tests for `MancerMap` are located in `Kick/tests/platform/MancerMapTests.cpp` and integrated into the AudIO CMake build via CTest:
```bash
cmake -B build -S AudIO -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja
ninja -C build mancer_map_tests
ctest --test-dir build --output-on-failure
```
All assertions pass with 100% test coverage for parsing, macro lookup, and parameter binding.
