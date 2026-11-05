# Kick app - screen / page / dialog inventory (from code only)

Date: 2026-11-04. Worktree `kick-plans` (HEAD 35fce454), read-only static survey. No build, no run, no hardware.
All paths relative to the repo root; `M/` = `Kick/modules/`, `A/` = `Kick/apps/`.

Status legend: KESZ = wired, working logic visible in code (not run); RESZLEGES = partly wired / works only on a subset;
STUB = UI exists, logic empty, hidden, commented out, or engine ignores it; BUG? = concrete suspicious code (evidence given);
N/E = NEM ELLENORIZHETO KODBOL.
"KESZ" always means "no stub marker and a handler that reaches the engine was found"; nothing was executed.

## 1. Entry points

| App | File | What it is |
|---|---|---|
| `kick` (the real app; Mac, Linux desktop, MPC Key 61, iOS) | `A/kick/Main.cpp` `AudioEngineApplication` (:129) | monolith: UI + engine in one process. START_JUCE_APPLICATION at :805 |
| `kick_main` | `A/kick_main/kick_main.cpp` (:30 `KickMain`) | IPC supervisor of the original Behringer multi-process design (log, heartbeat, exit handlers). No UI. `onHeartBeatNotReceived` (:85) restarts the whole subsystem. Not used by the monolith/MPC path. |
| `kick_gui` | `A/kick_gui/kick_gui_main.cpp` (:115 `GUI : IPCApplication`) | IPC-client GUI process (same `KickHome` content in a `DocumentWindow` :233-241). Original multi-process design. |
| `mpc_gui` | `A/mpc_gui/main_gui.cpp` (1686 lines) | standalone framebuffer experiment (own pixel drawing, 7 params SEL_PARAM_*, :188). NOT in any CMake (`A/CMakeLists.txt` has no `mpc_gui`). |
| `mpc_piano` | `A/mpc_piano/*` | ALSA/fb probes + additive synth experiment; separate CMake, not in the build. |
| `kickvst` | `A/kickvst` | VST3 template stub, only with `KICK_VST_PLUGIN`. |

Monolith start sequence (`A/kick/Main.cpp:203-367`): crash handler writes `/data/kick.crash` (Linux, :205-256) -> `KickAudioInterface` +
`KickAudioEngineAdapter` (:257-258; `A/kick/kick_audio_interface.h` is a dummy: createObject returns a random int, getProperties returns a
dummy, execute returns empty var - STUB, harmless because the UI uses `mt::getAudioInterface()`) -> `KickDeviceAdapter` -> fallback display
(:265) -> `mt::getSettings()` (:274) -> `mt::getAudioInterface()` (:277) -> `HomeWindow` (:280) -> visible only if `!smoke && hasDisplay` (:316)
-> on Linux `KickHardwareBridge::start()` unless `--smoke-test` (:323-329).
Smoke mode (`--smoke-test`, `KICK_SMOKE_SCREENS`, `KICK_SMOKE_SHOTS_DIR`) can screenshot 24 screens (:151-201).

## 2. Screen registry and navigation

Registry: `M/kick_ui/dialogs/kick_AppHome.cpp` `KickHome` ctor. Only `RecentProject` is created synchronously (:262); all others are created on a
thread-pool job + `callAsync` (:284-315, "lazy"). Enum `M/kick_ui_common/source/kick_IScreen.h:7-47`.

Registered keys (30): RecentProject, HomeListView(AppMainmode), HomeGridView(AppGrid), HomeSequencerView(StepSequencer), HomeXYView(XyFxView),
HomePreferencesView(Settings), HomeOptions(AppMenuOptions), MenuSampleEdit, MenuProgramEdit, MenuSampler, MenuLooper, MenuPadMute, MenuTrackMute,
MenuPadMixer, MenuChannelMixer, MenuKnobEdit, MenuListEdit, MenuPadPerform, MenuMidiControl, MenuFileBrowser, MenuNextSequence, MenuSong,
MenuColorEdit, MenuAssignSample, SWUpdateView, SWUpdateWindow, ProcessEdit, CVClib, Inserts.
Enum values with NO screen behind them (STUB, `load()` silently falls back to HomeListView, :353-357): `HomePreferences`, `EditProgram`,
`SystemResources` (the resources dialog is a header popup, not a screen), `Temp` (`TempScreen` class :88-165 exists, registration commented out :280, :315).

Navigation inputs:
- Screen stack: `sigScreenOpen` pushes the current screen (except Options/ListView/Invalid) :200-210; `sigScreenClose(Invalid)` pops :212-231.
- Touch: `AppTopbar` (`M/kick_ui/dialogs/kick_AppTopbar.cpp:22-107`): Menu, Main(List), Grid, StepSeq, XY, | Play, Browse, Metronome, NewFile(Projects), Settings.
  The top bar is only shown by screens whose `requiresControlBar()` is true = **HomeListView and HomeOptions only** (grep of all overrides).
  Every other screen has its own `KickModeHeader` (back button, icon, resources, metronome, automation, settings, play/stop, auto-load).
- Menu grid `AppMenuOptions` (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:25-107`), 16 tiles: Browser, Track Mixer, Track Mutes, Sequences(NextSeq),
  ListEdit, Padmixer, Padmutes, Songs, Coloreditor, Sampler, Sample editor, Program edit, Padnotes, Looper, Qlinks, Midicontrol.
  NOT in the menu: Grid, StepSeq, XY, Settings (top bar / hardware buttons only), Inserts, AssignSample, Projects.
- Hardware (DCL button codes) `KickHome::handleCommand` (:445-593): BROWSE, PAD_MIX, TRACK_MIX, PAD_MUTE, TRACK_MUTE, NEXT_SEQ, FX, STEP_SEQ, LOOPER,
  SAMPLER, SAMPLE_EDIT, LINK_EDIT, PROG_EDIT, MIDI_CONTROL, NOTES, TC (->ListEdit), BANK A-H, HALF/FULL LEVEL, LINK(long press), SAVE; MENU, MAINMODE,
  GRID, PREF (also while MidiControl is open). ALL of this runs only `if (prjLoaded && !isProjectLoading)` (:487): with no project loaded the front-panel
  buttons do nothing; only system messages and the Data Dial (centre knob) reach RecentProject (:587).
- MPC raw buttons are mapped to those codes (and Shift variants) in `A/kick/kick_hardware_bridge.h:898-1397`.

## 3. Startup screen (exact behaviour)

### 3.1 What exists
- There is NO splash / logo / boot animation / progress bar / "loading engine" text anywhere in the app code (grep splash|boot|logo|welcome in
  `A/kick/*`, `M/kick_ui/dialogs/*`: only `startup` in AssignSample and a comment in AppHome).
- First view = `ui::RecentProject` ("Projects", `M/kick_ui/recent_project/kick_RecentProject.cpp`): header "Projects", File-browser button, three horizontal
  lists of 200 px tiles: DEMOS (`getAppDirs(MTDemos)`), TEMPLATES (`MTTemp`), RECENT projects (reverse order) + a "New project" button.
  Tile = png thumbnail from `recent_project/` or folder icon + date/time. No status bar, no control bar (`kick_RecentProject.h:125-126`).
  Back arrow shown only if a project is already loaded (:428).
- Header extras: **Auto Load toggle** (:55-74, popup "Automatically load last opened project on startup?").
- Interactions: tap = (after double-click timeout +5 ms) load; long press = popup Load/Remove (remove refused for the current project :47);
  demo tile -> `LoadProjectAndAnimation`; template tile -> virtual keyboard asks a name -> copy `.kickPrj` -> load (:87-89, :158-170);
  New -> virtual keyboard (default `namePrefix`) -> `createProject` (duplicate-name check :109-124).
  Data Dial rotate = move selection, press = open (`knobRotated/knobClicked` :365-423).
- Load path: `startLoading` (`kick_RecentProject_Utils.cpp:20-26`) sets `isProjectLoading`, runs `loadProject` on a worker thread, shows `LoadAnimation`
  (13 PNG frames `animation/Load Animation B - NN.png`, 22 Hz, dark overlay + spinner fallback, `kick_utils_loadingAnimation.cpp`), on `onProjectLoaded` waits 200 ms
  and `sigScreenOpen(HomeListView)` (:28-37). Engine load contains a fixed 500 ms sleep (`mt_audio_manager.cpp` loadProject) and, when a project is already open, 100 + 500 ms more in `closeProject`.
- Auto load: first time RecentProject becomes visible, if `isAutoLoad` and `currentProject` non-empty -> loads it (:442-456).
- Duration: not time-boxed in code on desktop (waits for user or auto-load).

### 3.2 On the MPC Key 61 the above is overridden by the hardware bridge
`A/kick/kick_hardware_bridge.h:674-684` (timer 30 Hz, started :814): at **frame 30 (~1 s after start)** the bridge
1. forces `sigScreenClose(HomeListView)` (Projects screen is left after ~1 s whether or not the user touched it),
2. reloads the theme (`loadTheme("Default")`, a second time after :778),
3. if `/data/demos/Demo 01/Demo 01.kickPrj` exists, calls `mt::getAudioInterface().loadProject()` **synchronously on the message thread** (UI frozen for the
   >= 0.5 s fixed sleep in loadProject, +0.6 s more if a project is already open, + load time), without `LoadProjectAndAnimation`, without `isProjectLoading`, without the animation.
4. frame > 40 and a project is loaded (static one-shot flag :686-691): one more `sigScreenClose(HomeListView)` to refresh.
Consequences (BUG? / missing feedback):
- If autoload is on AND the demo exists, two `loadProject` calls can run concurrently (worker from RecentProject autoload :442-456 vs bridge on the message
  thread). `loadProject` has no re-entrancy guard besides `closeProject` (`mt_audio_manager.cpp:196-199`). N/E whether it actually collides (timing).
- Lazy screen creation race: `load()` for a screen not yet in `screens_` falls back to HomeListView (:353-357), and `setupLoadScreenVisibility` does nothing when
  `screens_[currentScreen]` is null (:359-366, and `operator[]` even inserts a null entry). The ~29 screens are built on a thread pool + callAsync after the ctor
  (:284-315). If frame 30 (or a fast auto-load: `onProjectLoaded` + 200 ms) fires before `AppMainmode` exists, `currentScreen` becomes HomeListView but the
  previous screen stays visible and nothing re-triggers (no demo -> no second refresh). N/E on real timing; MPC is ARMv7.
- No error surface: `LoadProjectAndAnimation::run` on failure only `stopAnimation()` (`kick_RecentProject_Utils.cpp:44-47`); no popup/notification. Save
  failures DO have a popup (`KickHome::onProjectSaveFailed` :320-325).
- No on-screen feedback for: framebuffer/touch/MIDI/ALSA init failure (adapter `init()` only `perror`, return value ignored `kick_hardware_bridge.h:780`),
  audio device failure (`AudioDevice::start` stores `error`, `AudIO/src/juce_adapter.cpp:18-23`; no UI reads it - grep in `M/kick_ui*` finds nothing),
  engine init, missing fonts/assets.
- No project loaded + MPC: front-panel buttons dead (see 2). Only touch + Data Dial work.
- Desktop Linux also starts the bridge (`Main.cpp:323`); `HardwareDeviceFactory::create` (`A/kick/hardware/device_factory.h`) falls back to
  `AkaiMpcKey61Adapter` on any machine ("Defaulting to AkaiMpcKey61Adapter"). `NiKontrolAdapter` (`ni_kontrol_adapter.h`) is a pure STUB (prints READY, blit/led empty).

## 4. Per-screen tables

Common chrome: `KickModeHeader` (`M/kick_ui_header/header/kick_modeHeader.cpp:30-130`): Back KESZ, System-Resources dialog KESZ-but-purge-STUB (see Resources), Metronome KESZ,
**Automation button STUB** (`automation.onClick = [&]{}` :54), Settings/NewProject/AutoLoad/PlayStop callbacks KESZ (when set by screen).
Status bar `AppStatusbar` (`kick_AppStatusbar.cpp`): STP (time-sig popup) + BBT (position popup) + info box; KESZ. Layout: 1024x600 logical.

### 4.1 Projects (RecentProject) - `M/kick_ui/recent_project/kick_RecentProject.{h,cpp}`
| Control | Status | Evidence |
|---|---|---|
| Demo / template / recent tile lists, thumbnails, scrolling | KESZ | :228-354 |
| Load project (touch / Data Dial) | KESZ | :339-348, :376-381 |
| Long-press Load / Remove popup | RESZLEGES | hard-coded pixel offsets `+432`, "TODO - Needs to be changed once finalist" :323-328 |
| New project (name keyboard) | KESZ | :91-104, dup check :109-124 |
| Template -> project copy | BUG? | `copyProjIntoProjectDir` :158-170: `sourceFiles[0]` unchecked (crash if template has no .kickPrj); no duplicate-name check (the check exists only in `createProject`), `copyFileTo` overwrites an existing project file of the same name |
| Auto-load toggle + auto-load at first show | KESZ (but see 3.2) | :55-74, :442-456 |
| File browser button | KESZ | :76-78 |

### 4.2 Main mode / HomeListView - `M/kick_ui/dialogs/kick_AppMainMode.{h,cpp}`
Tabs Sequence / Track / Program (default Track `tabSelected{1}` .h:48), toolbar Add / Delete / Undo / Redo (:169-257). Pad hits -> live MIDI to selected track (:406-420).
| Control | Status | Evidence |
|---|---|---|
| Add sequence / track / program (7-type chooser: Drum, Key, Clip, Audio, Plugin, Midi, CV) | KESZ | :169-200 |
| Delete (no confirmation) | KESZ | :202-220 |
| Undo / Redo (one global UndoManager for all three tabs) | KESZ | :222-257 |
| `handleAsyncUpdate` | STUB | empty, body is a comment `//refreshLEDs()` :476-479 |
| Sequence list rows (name, BPM, loop, SEQ/GLO, transpose, options) | KESZ | `M/kick_ui/sequence_view/source/kick_AppSequenceComponent.{h,cpp}` :55-56 |
| Sequence options: Half/Double length, Insert bars, Copy events, Copy bars, Copy sequence, Clear | KESZ | `kick_AppSequenceOptions.cpp:355-440` |
| Sequence option **Time Signature** | STUB | `case 2:; // Time Signature` then break, :366 (tile is in the menu) |
| Sequence option Bounce | STUB (commented out) | :369-377, menu line commented :539 |
| Track rows: name, program selector, length, transpose, level, mute, solo, rec-arm, I/P MIDI port+ch, meter | KESZ | `track_view/source/kick_AppTrackComponent.cpp:75-100` |
| Track **Monitor** button | STUB | added+skinned+placed, no handler anywhere (`kick_AppTrackComponent.cpp:26,136,272`; grep "monitor" finds nothing else) |
| Track options: Half/Double speed, Explode, Pitch quantize, Humanize, Split, Copy track, Random, Erase | KESZ | `kick_AppTrackOptions.cpp:172-300` (Bounce commented :136, :192) |
| Track option Split: `delete p` only on APPLY (dialog leaked on cancel) | minor BUG? | :212-236 |
| Program rows: name, route, assign, edit, sampler, MIDI ch/bank/port, CV ports | KESZ | `program_view/source/kick_AppProgramComponent.cpp:55-70, 143-147` |
| Program "edit" for Keygroup/Midi/Cv goes to Pad Perform; Drum -> Sample Edit; Clip/Audio -> Program Edit; Plugin -> CLAP modal | KESZ | :58-70 |
| Program options: Copy pads, Duplicate, Duplicate to track, Pad colour | KESZ | `kick_AppProgramOptions.cpp:43-70` |

### 4.3 Menu (HomeOptions) - `kick_AppMenuOptions.cpp` : KESZ (16 tiles route correctly :71-96; Play/PlayStart/Stop buttons :45-58).

### 4.4 Grid (HomeGridView) - `M/kick_ui/audio_components/gridview/*`
Piano-roll/drum grid, notes column, time-div list, scale ruler with seek, playhead, parameter lane (Velocity / Probability / Ratchet), tools Pencil / Erase / Select /
Magnify / Copy / Cut / Paste (`kick_AppGridHeader.cpp:24-82`), zoom +/-/default, edit-start/end/transpose/snap/nudge toggles, locate dialogs, 12-entry "Arrangement edit" menu.
| Control | Status | Evidence |
|---|---|---|
| Tools, zoom, parameter lane, seek, locate | KESZ | `kick_AppGrid.cpp:295-345` |
| Edit menu: Clear, Double length w/ events, Half speed, Double speed, Pitch quantize, Humanize, Random events, Split | KESZ | :351-381 |
| Edit menu **Trim To Length** | STUB | `case 4: //Trim To Length is Related to doubleLengthWithEvents break;` :368-370 |
| Edit menu **Convert To Progression**, **Bounce To Sample**, **Bounce To Audio Track** | STUB | `case 9/10/11: break;` :383-385 (buttons visible, `kick_AppGrid.h:26`) |
Note: the Grid has neither control bar nor status bar (`kick_AppGrid.h:287-288`).

### 4.5 Step Sequencer (HomeSequencerView) - `.../step_sequencer/*`
Two views (CLASSIC step bars with velocity/prob/ratchet, GRID drum grid; `viewModeButton`), track + track-length lists, total bars, BPM, pad/bar/page selectors,
Clear Pad / Clear Bar, nudge, step presets, TRK VOLUME. Hardware: pad buttons toggle steps, bank buttons = page.
| Item | Status | Evidence |
|---|---|---|
| Main functions | KESZ | `kick_stepSequencer.cpp`, `kick_StepSequencerControl.cpp:77-78` |
| Page selection by bank buttons when steps/page != 16 | RESZLEGES | `//TODO - Bank Updation not proper 4T,8T,16T...` :879 |
| No track yet | warning label "Create any Track Before using StepSequencer" | `kick_StepSequencer.h` |

### 4.6 XY FX / Touch FX (HomeXYView) - `.../xyfx/kick_XyFxView.cpp`, `M/kick_ui_components/source/kick_XYFXGrid.cpp`
Controls: XY pad, preset list, Attack / Release / Wet-Dry sliders (Q-Link 1-4), Output list (Out 1 / Out 2 / 1+2), On/Off, Latch, Setup panel, axis labels, header with Automation.
**STUB (UI only)**: the only engine calls in the whole XY module are Play/PlayStart/Stop (`kick_XyFxView.cpp:191-197`); `xyFx.onValueChanged`
only updates two labels (:96-98 region, `onValueChanged` -> `setXYCoordinateValues`); presets/attack/release/wetDry/output/on-off/latch change no audio or MIDI state.
(The separate MIDI Control > XY tab does send MIDI, see 4.19.) Header Automation button is dead (header STUB above).

### 4.7 Preferences (HomePreferencesView) - `M/kick_ui_settings/*`, 7 pages (`kick_ui_settings.cpp:88-96`; SettingsAudio is commented out :93 and its file is empty stubs)
| Page | Controls | Status | Evidence |
|---|---|---|---|
| Info | logo, serial, surface/firmware version, build stamp, Factory Reset, Firmware Update, Restore Defaults | RESZLEGES | Factory reset runs `rm -rf /home/Kick/projects* /home/Kick/prefs` (`M/hardware/source/kick_Hwpref.cpp:29,136`, only `if hardwareEnabled()`; MPC build has `KICK_ENABLE_HARDWARE=ON`, `docs/hardware/key61/tools/build_arm.sh:49`): board-image paths, not the MPC's `/data/...`; Restore Defaults KESZ (`mt::getSettings().makeDefaults()`); serial/versions from Behringer board (N/E on MPC) |
| Project Defaults | startup action, recent count, auto save + timeout, bit depth, tempo/bars/time sig, root/scale, seq/track count, default name | KESZ | `kick_ui_settings_defaults.cpp:34-100` |
| Hardware | pad threshold/sensitivity/curve, velocity, aftertouch, brightness, tap-tempo LED + taps, date, time, timezone | RESZLEGES / BUG? | pad settings are sent as a DCL packet (`SendPadSettings` :139); the MPC bridge's `onDclCommand` consumes only EN_PAD_SET_AFTER_TOUCH and EN_PAD_SET_COLOUR_STATE (`kick_hardware_bridge.h:137-160`) -> threshold/sens/curve have no effect on MPC; brightness writes `/sys/devices/platform/backlight/...` (Hwpref.cpp:841-853, Behringer path); **setDate/setTime execute the bare strings "2025-5-15" / "10:30:00" as shell commands - the `date -s` prefix is missing** (`kick_Hwpref.cpp:941, 959`); **year handler passes the year as month** `getDaysInMonth( Value, date_YYYY.getValue() )` (`kick_ui_settings_hardware.cpp:110`, compare :104) |
| MIDI | input port, output port, track/control toggles | RESZLEGES | page has no sync/clock controls despite name; `refresh()` restores only the INPUT selection (`kick_ui_settings_midi_sync.cpp:101`), output port selection is not restored |
| CV | CV scale/calibration page + `CVClib` screen | N/E | `kick_ui_settings_cv.cpp` (not read in depth); MPC CV hardware not wired in the bridge |
| Ethernet | enable, DHCP/manual, IP, subnet, gateway, DNS | RESZLEGES | `ifconfig/udhcpc` via Hwpref; DNS field is never applied (`update()` :153-158 ignores it; `refresh` sets "0.0.0.0" :127) |
| WiFi | enable, scan, SSID list, password, connect | RESZLEGES / BUG? | `refresh()` fills the **gateway** field with `info.subnet` (`kick_ui_settings_wifi.cpp:104`); list-refresh code is `#if 0`; `mt::WifiManager().getWifiInterface()->` dereferenced unchecked (:75-79) |
Also: `visibilityChanged` logs `"Settings " + visible ? ...` (pointer arithmetic, cosmetic, `kick_ui_settings.cpp:183`). Settings page timer refreshes the current page at 1 Hz.
Firmware Update (Info) -> `SWUpdateView` (4.21).

### 4.8 Sample Edit - `M/kick_ui/audio_components/sample_editor/*`
Waveform view with scale, region/slice editing; option pages: Chop (manual/threshold/regions/BPM, link, S/C), Trim (start/end/semi/fine/x-fade/tune/BPM, repeat, loop lock, link slice),
Program (root note, from-BPM), Tune (detect BPM, to seq), Slice/Combine; "Process" screen (14 actions: Discard, Delete, Silence, Extract, Normalize, Reverse, Fade in/out,
Pitch shift, Time stretch, Gain, Copy, Bit reduce, Stereo->Mono; `MTSampleEditorAction`, all dispatched in `M/engine/audio_engine/engine/mt_sample_editor_interface.cpp:207-219`).
| Item | Status | Evidence |
|---|---|---|
| Editing + processing | KESZ | `kick_SampleEditorProcess.cpp`, engine file above |
| `Regions::loopSelectedRegion()` | STUB | empty body `kick_SampleEditorData.cpp:232` (declared .h:262, no callers) |
| Chop/Trim/Program `buttonClicked(...) {}` | STUB (empty, parts are driven by per-control lambdas instead) | `kick_SampleEditorOptions.h:64,116,153` |
| With a slice selected, Discard/Delete/TimeStretch/Copy/BitReduce/Stereo->Mono are disabled | by design | `kick_SampleEditorData.cpp:1177-1182` |
| `kick_SampleEditorLayer.cpp:34` "TODO handle layer changes through ParameterQueryInterface" | RESZLEGES | layer change path |
| ppq hard-coded 960 ("todo get ppq from settings") | minor | `kick_SEScaleComponent.cpp:124` |
Sample Process (ProcessEdit) screen: KESZ (`kick_SampleEditor.cpp:311-420`).

### 4.9 Program Edit - `M/program_edit_ui/*`
Per type: Drum (Main, Samples, Layer Mix, Envelopes, LFO + Inserts, Flatten pads, pan/velocity), Keygroup (Master, Sample tab 1/2, Layer Mix, Filter/Env, LFO), Clip (program tab + pad tab),
Audio (grid with Select/Pencil/Erase/Cut/Copy/Paste/Magnify, cut/drag tools), Midi, CV (read-only voltage sliders, `kick_CvGateProgramEdit.cpp:28-34`, `refresh`/`handleCommand` empty :46-52).
Plugin type: no editor in the switch (`kick_ProgramEdit.cpp:166-173`); edited from the program row as a CLAP modal (`kick_ClapPanel.h`: scan, add folder, load, remove, panic, errors,
plugin list, search, parameter rows, 4 macros on Q-Links; no native plugin GUI, `clap_host/README.md`).
| Item | Status | Evidence |
|---|---|---|
| Drum program editors | KESZ | engine `DrumVoice` has ADSR/filter/LFO (`docs/planning/plans/research/codebase-survey.md` s.1) |
| **Keygroup Filter/Env, LFO, Layer-Mix pages** | STUB (UI bound to model, engine ignores) | `M/engine/audio_program/synth/KeygroupProgram.cpp`, `audio_voice.h`, `iSynthVoice.h` contain no adsr/filter/lfo (grep empty); survey s.1 confirms. Note Layer-mix: only VELOCITY layerPlay is honoured |
| Midi, Clip, Audio editors | KESZ (not read in depth) | |
| CV editor | read-only monitor | see above |
| Sample offset widgets disabled until a mode != "None" | by design | `kick_SampleOffset.cpp:30-45` |

### 4.10 Sampler (record) - `.../sampler/*`
Modes Sample / Slice / Pad Tap / Pad Hold, Slice+, Cancel, Save, Play, Discard, Edit, input/gain(threshold)/monitor options, meter, max-length timer.
| Item | Status | Evidence |
|---|---|---|
| Record, slice marks, pad tap/hold, save/edit popups, preview | KESZ | `kick_Sampler.cpp:72-130, 380-420` |
| Header **Export** | STUB | `// Export The Current Sample` empty branch `kick_Sampler.cpp:56-62` |
| Header **Clear** | STUB | comment only `:63-66` |
| `options.outputChanged` | STUB | empty lambda `:38-39` |
| Pad-tap pad colour toggle | STUB | `if( 0 )` + `//TODO--> Need PadButton Colour from GD side` :411-420 |
| Record length | RESZLEGES | `startRecording` has `//TODO` and passes fixed args to `setupRecorder` (`kick_SamplerRecorder.cpp:344-353`); ppq hard-coded :137 |
| Audio INPUT on MPC | N/E | recorder uses the engine's JUCE `AudioDeviceManager` input channels (`AudIO/src/juce_adapter.cpp:22`); whether the MPC ALSA device exposes inputs is not decidable from code. Older audit `docs/hardware/key61/todo.md` s.1 (2026-10-31, static) lists audio input as missing |

### 4.11 Looper - `.../looper/*`
Input selector (6 L/R modes), inserts button, recorder with bar display, Export (name popup -> wav -> asset -> optional program/pad), Clear; hardware REC/OVERDUB.
KESZ (`kick_Looper.cpp:27-80`); ppq hard-coded 960 `kick_LoopRecorder.cpp:275`; same input question as 4.10.

### 4.12 Pad Mute / 4.13 Track Mute - `M/kick_ui/modes/kick_padMute.*`, `kick_trackMute.*`
Mini-map pads, mute/group tabs, solo, clear solos, instant vs time-division, play/record mute-solo events settings. KESZ (handlers present, no stub markers).

### 4.14 Pad Mixer - `modes/mixers/kick_PadMixer.cpp`
Per-pad level/pan/solo/mute with meters, sample list; info label "PAD MIXER IS NOT APPLICABLE" for non Drum/Clip programs (:422).
| Item | Status | Evidence |
|---|---|---|
| Level/pan/mute/solo | KESZ | |
| **Pad meters** | BUG? | every pad shows the PROGRAM output meter (`//TODO - Need to call different API for Pad` :532) and the right peak is fed the left peak: `outMeter[0].peak, outMeter[0].peak` :536 (should be `[1]`) |

### 4.15 Channel (Track) Mixer - `M/kick_ui_channel_mixer/*`
Tabs: Programs, Sends, Returns, Submixes, Output, Midi, Audio + master route strip, inserts buttons. KESZ for touch use.
**BUG?** `ChannelMixer::knobCommand` (hardware Q-Links in Screen mode): the view order is Output(4), Midi(5) (`kick_ui_channel_mixer.cpp:71-72`) but the switch casts
index 4 to `TrackMidiMixer` and 5 to `OutputTrackMixer` (:173-177); `dynamic_cast` yields nullptr -> null dereference when turning a Q-Link on those two tabs.
(Default knob mode is Screen = 1, `kick_settings.h:218`, so this path is live.)

### 4.16 Knob Edit / Q-Links - `modes/knobedit/*`
Screen mode (fixed to the current screen) / Custom mode (assignable), track list, program-type label, per-knob grid, long-press on LINK. KESZ (`kick_KnobEdit.cpp`).
Note: `KickHome` dereferences `getScreen(MenuKnobEdit)` / the `MenuAssignSample` cast without null check (`getScreen(MenuKnobEdit)->knobCommand` :569, `dynamic_cast<KnobEdit*>(screens_[MenuKnobEdit].get())->knobLongPress()` :542, AssignSample cast :426); screens are created lazily (3.2).

### 4.17 List Edit - `modes/kick_ListEdit.cpp`
Event table (pad, start, length, vel...), track selector, filter, Insert dialog, Delete, Snap, **Timing Correct dialog** (global enable + parameters; "range not supported" notification when engine returns <0, :164). KESZ.
Delete does not guard "no row selected" (`getChild( tableView->getSelectedRow() )` :124 region) - minor.

### 4.18 Pad Perform (Notes) - `modes/padperform/*`
Chromatic / Notes (root, scale) / Chords / Progressions; start-on bank/octave; bank +/-. KESZ; ~74 lines of commented-out octave inc/dec code (`kick_PadPerform.cpp:180-232`).

### 4.19 MIDI Control - `modes/midicontrol/*` (3084-line tabs file)
Tabs Hardware (buttons / Q-Links / pads -> MIDI messages, output port), XY (grid sends two MIDI messages on drag), MIDI Learn (mapping table, enable), MIDI Monitor dialog. KESZ (not read in depth).

### 4.20 File Browser - `modes/filebrowser/*`
Places (Content / Expansion / SD / USB), file table, Project / Sample / All filters, search, back, cut/copy/paste/rename/delete (hidden in the default filter,
`mt::utils::makeHidden( cut, copy, paste, rename )` :247, delete visible), Select, audition OFF/AUTO/MAN + play + volume, Load, Load All, Sample Assign, favourites.
| Item | Status | Evidence |
|---|---|---|
| Browse/search/load/audition/assign, MPC mount discovery (`/proc/mounts`, `/sdcard`, `/media/<label>`) | KESZ | `kick_fileUtils.cpp:8-60` |
| Folder-size calculation (async) | KESZ | `kick_fileUtils.cpp:62-` |
| **Eject** | STUB / BUG? | only removes the list row and sets `isSdCardEject` flags - no `umount`/`eject` call anywhere (`kick_fileBrowser.cpp:408-440`, `kick_fileUtils.cpp:78-93`); the user may pull the device believing it was flushed |
| **Format** | STUB | popup whose callback only deletes the popup, "need to implement the functionality"; the button is not even added (`kick_fileBrowser.cpp:1624-1628, 1664`) |
| Hard-coded fallback `/mnt//flash` (double slash) | minor | `kick_fileUtils.cpp:35,39` |

### 4.21 Next Sequence / 4.22 Song / 4.23 Color Edit
- Next Sequence (`modes/kick_nextSequence.cpp`): sequence table, Next Bar / Sudden / Clear / Hold, pads queue. KESZ.
- Song (`modes/kick_song.cpp`): song list, steps table, Insert / Delete / Clear, BPM Seq/Song, transport. Insert/Delete/Clear KESZ.
  **Export** STUB: dialog opens, `if( R == UI::APPLY ) {}` empty (:411-423). **Convert >SEQ** STUB: member created and added (:85) but no handler, bounds commented out (:283) -> invisible.
- Color Edit (`modes/kick_PadColor.cpp`): single/all pads, pad off (Dim/Off) and on (Velocity/colour), palette, make default, bank nav. KESZ.

### 4.24 Assign Sample - `dialogs/kick_AppAssignSample.cpp`
Drum: pad list + file list + software pads; Keys: key table + audition/clear/load; bank nav; file-browser jump. KESZ. `startup()` entry used by KickHome (:426).

### 4.25 Inserts - `dialogs/kick_Inserts.cpp`
Insert slots per program/bus/looper (list + enable/bypass + add/remove, selector, per-effect options). KESZ. Header Automation button dead (header STUB).

### 4.26 SW Update (SWUpdateView, SWUpdateWindow) - `M/kick_ui/swupdate/*`
Lists SD/USB/PC images (thread `ImageCheckingThread`), validate, apply, progress window. Behringer firmware flow (`/usr/bin/swupdate -i /tmp/fwupdate/Kick.swu`, MCU hex update) - RESZLEGES on MPC:
`SWUpdateView::handleCommand` `{ /*TODO*/ }` (`kick_SWUpdateView.h:93`); `linuxUpdate` has two unbounded `while(1)` wait loops (`kick_FirmwareUpdate.cpp:453, 467`);
an old commented-out `visibilityChanged` copy sits in the .cpp (:~245). The Info > Firmware Update button is reachable on the MPC build; whether images validate there is N/E (images are Behringer-signed/encrypted).

### 4.27 System Resources (header popup) - `M/kick_ui_systemResources/*`
CPU, RAM, flash, SD, USB meters at 1 Hz; **Purge All / Purge Unused STUB** (`purgeAll.onClick = [&](){}` :78-79); OK closes. Flash gauge uses `internalPath` (Hwpref.cpp:889 `/home/Kick`) -> meaning on MPC N/E.

### 4.28 Pop-ups / dialogs
Metronome popup (level, enable, mode, count-in, rate, style; `kick_modeHeader.h:90-110`) KESZ; Time-signature (STP) and Position (BBT) popups KESZ; Locate dialogs
(`kick_Locate.cpp`; `LocateDialog::buttonClicked` empty :124/:304 - driven by other callbacks); virtual keyboard KESZ; FilePopup/ConfirmationPopup KESZ.

## 5. Hardware (MPC Key 61) vs desktop

| Aspect | MPC Key 61 (Linux ARMv7, no X) | macOS | Linux desktop (X) | iOS |
|---|---|---|---|---|
| Window | hidden `DocumentWindow` (`Main.cpp:316`), no JUCE focus/`isShowing`; bridge paints the tree into an ARGB 1280x800 image and blits to `/dev/fb0` at 30 Hz (`bridge:661-751`); UI logical 1024x600 scaled | `HomeWindow` with `KickHardwareChassis` (3 view modes: Hardware chassis 700x710, Display only 1024x600, Mobile T-layout 852x393), menu bar, scale 100/120/140/fit, F1-F3/F5, Cmd+D demo, Cmd+O projects, hardware drawer toggle (`Main.cpp:417-720`) | plain `DocumentWindow(KickHome)` kiosk, keyboard panel (`KICK_KEYBOARD_PANEL`) | `KickMobileTLayout` (`Main.cpp:723-755`); app exits at launch (`AGENTS.md` Known state) |
| Input | touch (multi-slot, double tap 450 ms/24 px), 45 buttons incl. Shift/Locate combos, Data Dial, 4 Q-Links, 16 pads (+aftertouch), 61 keys, pitch/mod/sustain/expression, touch strip (8 modes, `bridge:882-897`) | mouse on chassis image / keyboard map | mouse + keyboard map | touch |
| Boot behaviour | 1 s forced HomeListView + demo load (see 3.2) | Projects screen stays | same | N/E |
| Hardware drawer (`M/kick_ui/hardware_drawer/*`) | created, hidden, no toggle on MPC | menu item idToggleDrawer | none | N/E |
| LEDs | button/pad/RGB LEDs driven by the bridge (mode LEDs, bank, arp, octave, TC, automation) | none | none | none |
| `kick_cmd` FIFO (`/data/kick_cmd`, btn/touch/jog/pad/key/strip/rawmidi/snapshot) | yes (`bridge:818`) | no | created if bridge starts | no |
| Board commands (brightness, date/time, eth/wifi, poweroff, factory reset, swupdate) | executed (`KICK_ENABLE_HARDWARE=ON`) with Behringer-image paths | no-op (`hardwareEnabled()` false, `kick_HardwareMode.h`) | no-op unless built with the flag | no-op |
| Front-panel Note Repeat | button sends EN_NOTE_REPEAT; **no consumer** in UI/engine (grep outside keyboard_detector/chassis/bridge is empty) | same | same | - |
| Undo | direct UndoManager (`bridge` HW_BTN_UNDO) | menu/Top bar | | |
| ARP/LATCH | engine arp implemented (`ae.isArpEnabled/setArpLatch`) | mac chassis forwards | | |
| `NiKontrolAdapter` | auto-selected on Komplete/Traktor/Maschine card names; STUB | | | |
Bridge also drops all DCL commands except aftertouch-enable and pad-colour (`bridge:137-160`): LED packets from the UI (e.g. HALF/FULL LEVEL LED in `KickHome` :538-545) and pad threshold packets are ignored; the bridge drives those LEDs itself.

## 6. Consolidated register of stubs / suspected bugs (priority order)

1. BUG? Boot override on MPC: forced screen switch at frame 30, synchronous demo `loadProject` on the message thread, possible double load with autoload, no animation/isProjectLoading (`kick_hardware_bridge.h:674-691`).
2. BUG? Lazy screen creation vs `load()` fallback (`kick_AppHome.cpp:353-366, 284-315`).
3. BUG? ChannelMixer Q-Link null dereference on Output/Midi tabs (`kick_ui_channel_mixer.cpp:173-177`).
4. BUG? Template copy: unchecked `sourceFiles[0]`, silent overwrite of an existing project (`kick_RecentProject.cpp:158-170`).
5. BUG? `HwPref::setDate/setTime` run bare strings (`kick_Hwpref.cpp:941, 959`); year handler uses year as month (`kick_ui_settings_hardware.cpp:110`).
6. BUG? Pad Mixer meters (`kick_PadMixer.cpp:532-536`); WiFi gateway shows subnet (`kick_ui_settings_wifi.cpp:104`); MIDI output port not restored (`kick_ui_settings_midi_sync.cpp:101`).
7. STUB XY FX screen has no effect (4.6). STUB Keygroup Filter/Env/LFO (4.9). STUB Track Monitor, Sequence Time Signature, Grid 4 edit entries, Song Export/Convert, Sampler Export/Clear/Output,
   File-browser Eject (UI-only)/Format, System Resources Purge x2, header Automation, Sample-editor `loopSelectedRegion`, NOTE REPEAT button, `TempScreen`, `KickAudioInterface`, `NiKontrolAdapter`, `SettingsAudio`.
8. Missing feedback: no splash; no error popups for project-load failure, audio device failure, hardware init failure.
9. Board-image assumptions on MPC: factory reset path, brightness sysfs, `/home/Kick` flash gauge, swupdate.

## 7. Not verifiable from code
Real timing of the boot race (3.2); whether MPC ALSA exposes audio inputs (Sampler/Looper); CV pages on MPC; MIDI Control tabs and Audio/Midi/Clip program editors beyond skim;
Pad Mute / Track Mute / Next Sequence / Color Edit logic depth (no stub markers found, not traced into the engine); whether Behringer shell commands are present on the MPC rootfs.
