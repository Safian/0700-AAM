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
| - Lock-free SPSC event ring buffer (incoming ComProtocol / MIDI)     |
| - Dynamic loading of native aarch64 .clap binaries from SD card       |
| - 4x CV/Gate modulation outputs + Hybrid analog VCF/VCA loop          |
+-----------------------------------------------------------------------+
```

---

## 2. Declarative `.mancer` / `.json` Profile Schema

Instead of hardcoding parameter IDs inside the C++ audio thread, `MancerMap` allows users and sound designers to store declarative mapping files alongside `.clap` binaries in `/mnt/sdcard/Plugins/CLAP/` (or `/data/plugins/`):

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
