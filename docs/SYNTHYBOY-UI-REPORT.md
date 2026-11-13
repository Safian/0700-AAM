# The plug-in's own GUI in the Kick plug-in panel (client of `com.safian.bitmap-gui/1`)

Branch `claude/synthyboy-ui`, base `safian/main` b8b2a14a (which contains 6ca3eb1c). Contract and usage: `Kick/clap_host/README.md`, section "Plug-in UI (bitmap GUI)".
SynthyBoy side: SynthyBoyVST `develop` c9f0950 (`docs/BITMAP-GUI-EXTENSION.md`, `shell-clap/bitmap_gui_ext.h`).

## Decision this report follows

SynthyBoy stays a standalone module loaded as a `.clap`. Kick contains **no copy of SynthyBoy's UI or engine**. The first plan (vendoring `synthyboy_ui`, `engine-vital`,
`fx-serum-*` into `Kick/clap_host/builtin/synthyboy`, a sync script, `KICK_SYNTHYBOY_VITAL`) was built far enough to prove that the library compiles against JUCE 7.0.12
(277 steps, no error) and then dropped on the owner's decision. It was reverted in a normal commit (`2993c546`, a revert of `fdc258c3`); nothing of it is left in the tree.
The built-in, Vital-free SynthyBoy (`Kick/clap_host/builtin/synthyboy`) is exactly as on `safian/main`.

## What changed

| Where | What |
|---|---|
| `clap_host/third_party/safian/bitmap_gui_ext.h` + `upstream.json` | The extension header, verbatim from SynthyBoyVST c9f0950 (the header is meant to be copied). |
| `clap_host/include/kick_clap/Host.h`, `src/Host.cpp` | `Host::bitmapGui()`, `setBitmapGuiHandler()` (host-side table `com.safian.bitmap-gui.host/1`: a function the handler cannot do is NULL; calls from a foreign thread are ignored; the GUI is destroyed before the plug-in is, `pluginUnloading()`), `plugin()`, `clap.remote-controls` pages (`remotePages()`, host extension `changed` / `suggest_page`). |
| `clap_host/include/kick_clap/BitmapGui.h`, `src/BitmapGuiClient.cpp` | `BitmapGuiClient`: the generic client (no JUCE). |
| `clap_host/include/kick_clap/RemotePages.h` | Pure Q-Link logic: knob offset to page / slot, page stepping, page of a set of parameters, relative turn. |
| `clap_host/juce/BitmapGuiComponent.*` | The `juce::Component`: timer-driven tick, render only when dirty, dirty rectangles into one reused `juce::Image`, mouse / wheel / keys / touch (pointer id per finger), cursor, files, share, URLs, text input hooks, `HOST_RUNS_EVENT_LOOP` on macOS / Windows / iOS. |
| `clap_host/juce/PluginPanel.*` | Shows the plug-in's GUI when the loaded plug-in has the extension (below a 28 px strip: Parameters toggle, Q-Link page title, `<` `>`); the generic list stays the fallback and the toggle; in-panel touch file chooser for Linux / MPC; `adjustKnob()` for the Q-Link knobs 1..16. |
| `clap_host/juce/PluginSession.*` | `adjustParameter()`, background discovery, precedence swap (below). |
| `clap_host/include/kick_clap/PluginManager.h`, `src/PluginManager.cpp` | `resolve()`, `setDiscovered()`, `publishDiscovered()`, `registerDynamicPlugin()`; a revoked / expired package no longer shadows the built-in. |
| `modules/kick_ui/program_view/source/kick_ClapPanel.h` | Q-Link knobs 1..16 to `PluginPanel::adjustKnob`; text input = `KickVirtualKeyBoard`; share hook; touch slot provider (MPC bridge). |
| `modules/engine/audio_program/source/mt_audio_clap_program.cpp` | Starts the background discovery when a Plugin program is created. |
| tests | `clap_host/tests/BitmapGuiTests.cpp`, `PrecedenceSessionTests.cpp`, `tests/platform/QLinkBankTests.cpp` (extended), `tests/platform/ClapPanelSmoke.h` (`KICK_SMOKE_CLAP_PANEL`). |
| docs | `Kick/clap_host/README.md` (section), `docs/manual/USER_MANUAL.md` (CLAP section), `docs/README.md`, this report, `docs/images/synthyboy-bitmap-gui-1024x600.png`. |

## Precedence of a module over the built-in

Rule (`PluginManager::resolve`, applied by `PluginSession::select` and when a saved project is loaded), for a request of a built-in (`internal:...`):

1. an installed package of that id (PluginInstaller + manifest + TrustStore, unchanged) whose signature is not `Revoked` / `Expired`;
2. a `.clap` binary found by discovery: the isolated `kick-clap-scan` enumerated that id and the file exists (`/usr/lib/clap` on the MPC; `~/.clap`, `CLAP_PATH` and the folders of `KICK/clap-paths.txt` on the desktop);
3. the built-in.

A request that names a binary is never rewritten. `getAllLoaders()` lists an id that is both built in and installed once; `findLoaderById` agrees. Discovery runs once in the background when the first
Plugin program is created; a loaded built-in is swapped for the module when discovery finishes (state kept: `select()` saves and restores it); a project saved with the built-in opens with the module.
iOS (and any target without an installed module) keeps the built-in: it has no bitmap GUI, so those targets show the generic list, exactly as before.
Tested: `BitmapGuiTests.cpp::testPrecedence` (rule, trust, missing file, explicit path, id listed once) and `PrecedenceSessionTests.cpp` (swap with state kept, saved project, fallback).

## How to update the module

The module is built by the SynthyBoyVST repo; Kick only needs the new `.clap` in the plug-in folder (`/usr/lib/clap/synthyboy.clap` on the MPC). A new version of the header is copied verbatim into
`clap_host/third_party/safian/` (update `upstream.json`); an incompatible change would be `com.safian.bitmap-gui/2` and a second client.

## Verification

Verified (this Mac, arm64, Release; module = SynthyBoyVST `build/shell-clap/synthyboy.clap` of 2026-11-11 02:50, copied to `dist-test/synthyboy-macos.clap`):

| Gate | Result |
|---|---|
| macOS Ninja build of `Kick`, `kick-clap-scan`, `kick-clap-bench` (JUCE 7.0.12, `-DKICK_BUILD_PLATFORM_TESTS=ON`) | built, no new warnings in the new files |
| `Kick/clap_host` sanitizer suite (`-DKICK_CLAP_HOST_ONLY=ON -DKICK_CLAP_SANITIZE=ON`, `ASAN_OPTIONS=detect_odr_violation=0 ctest`) | 5/5: host, bitmap-gui (with the real module), scanner CLI, benchmark, benchmark CLI |
| `clap_host` with JUCE (`-DKICK_CLAP_BUILD_JUCE=ON`) ctest | 10/10: also session, precedence-session, persistence, hardware input, device benchmark |
| `QLinkBankTests` (ASan/UBSan, plus ctest) / `QLinkLogicTests` / `kick-clap-effect` / `kick-clap-processor` | pass |
| `BitmapGuiTests`: fake plug-in (capabilities follow the sinks, frames full / partial / unchanged, input ids and positions, text, focus, resize, scale, file / text / cursor / url / share / tick / resize requests, remote pages both ways, unload while open destroys the GUI once, foreign-thread calls ignored) | pass |
| `BitmapGuiTests` with the real SynthyBoy module through the dynamic path | first frame full 1024x600, opaque, not blank; idle stays clean (0 dirty of 2000 ticks, 1.0 us per `tick()+is_dirty()`); a tap gives a partial frame; resize; 159 remote pages read |
| Kick `--smoke-test` (plain) and the existing CLAP fixture smoke (`KICK_SMOKE_CLAP_PATH`) | pass |
| Kick `--smoke-test` with `KICK_SMOKE_CLAP_PANEL=<real module>`: plug-in loaded as a binary (not the built-in), panel shows the plug-in GUI (1024x572 under the 28 px strip), audio renders from a held note, Q-Link knob 1 changes the first parameter of the focused page, knob 9 the next page, knob 17 does nothing, the Parameters toggle shows the generic list and back | pass; screenshot `docs/images/synthyboy-bitmap-gui-1024x600.png` (panel 1024x600) |
| ARMv7 `./docs/hardware/key61/tools/build_arm.sh` | OK, `Kick` 20,101,732 bytes + `kick-clap-scan` 1,489,104 bytes; ELF 32-bit ARM EABI5 hard-float, VFPv4; NEEDED = `libasound`, `libfreetype`, `libstdc++`, `libm`, `libgcc_s`, `libc`, `ld-linux-armhf` (nothing new: no X11, no SynthyBoy libraries; the module is dlopened); highest symbol versions GLIBC_2.35, GLIBCXX_3.4.30 (`kick-clap-scan`: GLIBC_2.34) |
| ARMv7 SynthyBoy module with the UI (`scripts/build-armv7-clap.sh --jobs 2`, read-only checkout of develop c9f0950, build dir outside the repo) | OK, `check-clap-exports` OK (exports `clap_entry` and `synthyboy_clap_get_entry` only); NEEDED = `libstdc++`, `libm`, `libgcc_s`, `libc`, `ld-linux-armhf` (no X11 / ALSA / freetype) |

Measurements on this Mac (Apple M-series, software renderer, plug-in GUI 1024x572, smoke run, three runs): the whole Kick process uses about 1.9 % CPU with the plug-in loaded and no panel;
with the panel open and the GUI idle it uses about 4.3 .. 5.2 % (0 renders in 3 s, 45 ticks at about 15 Hz, `tick()+is_dirty()` 1 .. 2.5 us). With a held note and the audio running (meters / scope live):
28.7 .. 29.3 frames per second, 3.3 .. 4.7 ms per frame for plug-in render plus copy into the `juce::Image`, 226,000 pixels copied per frame (about 40 % of the area), 17.6 .. 23.5 % CPU of the whole process including the plug-in's audio.
**The about 2.5 percentage points of an idle GUI are not the client**: a `sample` of the idle process shows them in the plug-in's own JUCE timer `synthyboy::ui::ModMatrixPanel::timerCallback`
(it calls `modSlotParams()` -> `findParamByKey()` (a linear `strcmp` search) for every modulation slot at every timer tick, also when the panel is not visible). Needed change in SynthyBoyVST: cache the parameter
indices of the 64 slots once (a static table) and/or run that timer only while the panel is showing a change; that would bring an idle GUI to the client's ~0.

**MPC hardware speed is NOT measured** (no deploy, no ssh, as instructed). The ARMv7 module and Kick were only built.

Not verified:

- Anything on the MPC: the touch path (the bridge's slot as pointer id, finger slop, long-press 400 ms, two-finger pinch / pan), the on-screen keyboard flow (`KickVirtualKeyBoard` replaces the text field, Enter commits), the in-panel file chooser (never run, not even on the Mac: macOS uses the native chooser), render speed of the 1024x572 frame on ARMv7 (software renderer; the SynthyBoy report has emulated numbers only), `getComponent` hit testing of the plug-in GUI inside the scaled `dawStage`.
- iOS: nothing was built or run (phone sizes follow from the plug-in's layout by width, 360 .. 1800; iOS keeps the built-in, which has no bitmap GUI).
- Windows / Linux desktop builds.
- The precedence swap with the real SynthyBoy `.clap` installed in `/usr/lib/clap` of a real MPC (tested with the fixture plug-in and the fake plug-in; the real module only through an explicit path).
- Pointer kinds pen / `CANCEL`, wheel units (JUCE detent = 0.1, mapped to 1.0 notch; checked against the spec text only), key-up events (JUCE reports presses only: a down is followed by an up at once).
- Multi-finger pointer ids on desktop with a touch screen.

## Assumptions

- The plug-in GUI is created at the panel's size minus the strip (28 px): 1024x572 on the MPC. The extension allows any size; SynthyBoy's golden images are 1024x600, so the panel is a size nobody tested there (the screenshot looks right: tabs, mixer, wavetable, keyboard visible).
- Render scale 1.0 on Linux / MPC (the bridge scales the whole UI 1024 to 1280 itself), the display scale elsewhere (`KICK_BITMAP_GUI_SCALE` overrides).
- Q-Link: knobs 1..8 = focused page, 9..16 = the page after it; the page follows the plug-in's `set_active_remote_page` (and `suggest_page`), the `<` `>` buttons step through all pages and call `show_remote_page`.
- The text keyboard returns the whole text: the client sends Ctrl+A, then the text, then Enter into the plug-in's focused field.
- `request_resize` is honoured by asking the panel to resize; the dialog on the MPC is fixed at 1024x600, so a larger request is clamped by the plug-in.
- A module discovered but failing to load leaves the session on the built-in (the swap only replaces a loaded built-in, via `select()`, which keeps the old plug-in when the new one does not load).

## Needed from the SynthyBoy side

1. `ModMatrixPanel::timerCallback` cost at idle (above).
2. Nothing blocks the extension: the header, the real module and the spec matched in every test. The earlier brief asked for extra CLAP extensions for caps / content / MIDI learn; none is needed any more because the
   plug-in draws and owns all of that itself.
3. A hook in the bitmap GUI for the host to ask "what is the current page" would let Kick show the knob names of the active page; today the host shows the page title from `clap.remote-controls` only (optional).

## Risks

- A plug-in that draws slowly on ARMv7 holds the message thread: render is synchronous in `tick()` / `render()` (the contract). The component renders only when dirty and at most at the plug-in's `next_frame_ms`, but a 1024x572 software frame on the A7 is unmeasured.
- Two JUCE copies in one process (Kick's 7.0.12 and the module's 8.0.17): the module hides its symbols (`check-clap-exports`); on macOS the plug-in's timers ride the host run loop (`HOST_RUNS_EVENT_LOOP`), which is the documented case.
- Background discovery starts a scanner process per binary at the first Plugin program; on a slow device with many plug-ins this is the same cost as the panel's Scan button, once per start.

## Hardware control and MPC specifics (verified on the Key 61)

- **Q-Link**: encoders 1..4 times four banks drive the page of what is on screen (knobs 1..8) and the page after it (9..16); hardware `+` / `-` and cursor left / right step the pages (`PluginPanel::stepRemotePage`, taken through the panel's hardware claim, press and release); the page title is shown in the strip.
- **Undo / Shift+Undo** go to the plug-in (Ctrl+Z / Ctrl+Shift+Z key events) while its panel is open; otherwise the project's undo (`kick_clap/HardwareHooks.h`, asked by the bridge first).
- **Jog wheel** scrolls the plug-in GUI: a wheel event over the right edge (the scroll bar: a wheel over a knob would change the knob). Two-finger pan and the one-finger drag on a background (SynthyBoy `touch-scroll`) scroll as well; a drag on a control never scrolls.
- **No on-screen piano** on Linux / MPC: `SAFIAN_BITMAP_GUI_CREATE_NO_ONSCREEN_KEYBOARD` (the hardware keys play).
- **Render density**: the plug-in bitmap is rendered at the on-screen pixel density (the bridge scales the 1024x600 UI to 1280x800), otherwise it is enlarged and coarse.
- **User directory**: the MPC starts the app without `HOME`; Kick sets `SYNTHYBOY_USER_DIR=/data/synthyboy` (Linux, when not set). The factory presets go to `/data/synthyboy/packs/factory` with `docs/hardware/key61/tools/deploy_synthyboy_content.sh <SynthyBoyVST>/content/presets`; without them the preset arrows have nothing to step through.
- Not verified: sound output through the MPC (not listened to), every control of every tab (the tabs, top strip, OSC controls, wavetable editor, preset stepping, knob drag, Q-Link and pages were exercised with screenshots), the Q-Link touch strip and the physical encoders (injected through `kick_cmd` only).
