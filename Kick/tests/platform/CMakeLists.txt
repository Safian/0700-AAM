cmake_minimum_required(VERSION 3.16)
project(KickPlatformTests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(KICK_JUCE_PATH "" CACHE PATH "JUCE 7.0.12 source tree")
if(NOT TARGET juce::juce_core)
    if(NOT EXISTS "${KICK_JUCE_PATH}/CMakeLists.txt")
        message(FATAL_ERROR "Set KICK_JUCE_PATH to JUCE 7.0.12")
    endif()
    add_subdirectory("${KICK_JUCE_PATH}" juce EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET kick_clap_host)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../clap_host" clap_host EXCLUDE_FROM_ALL)
endif()
enable_testing()
add_executable(kick-serial-tests SerialTests.cpp ../../modules/serial_port/serial_port.cpp ../../../ComProtocol/PktMgr/PktMgr.cpp)
target_include_directories(kick-serial-tests PRIVATE ../../modules ../../../ComProtocol)
target_link_libraries(kick-serial-tests PRIVATE juce::juce_core)
target_compile_definitions(kick-serial-tests PRIVATE JUCE_USE_CURL=0)
add_test(NAME kick-serial COMMAND kick-serial-tests)
set_tests_properties(kick-serial PROPERTIES TIMEOUT 10)

add_executable(kick-mixer-logic-tests MixerLogicTests.cpp)
add_test(NAME kick-mixer-logic COMMAND kick-mixer-logic-tests)
set_tests_properties(kick-mixer-logic PROPERTIES TIMEOUT 10)

add_executable(kick-midi-parser-tests MidiParserTests.cpp)
add_test(NAME kick-midi-parser COMMAND kick-midi-parser-tests)
set_tests_properties(kick-midi-parser PROPERTIES TIMEOUT 10)

add_executable(kick-pitch-bend-tests PitchBendTests.cpp)
add_test(NAME kick-pitch-bend COMMAND kick-pitch-bend-tests)
set_tests_properties(kick-pitch-bend PROPERTIES TIMEOUT 10)

add_executable(kick-live-capture-tests LiveCaptureTests.cpp)
add_test(NAME kick-live-capture COMMAND kick-live-capture-tests)
set_tests_properties(kick-live-capture PROPERTIES TIMEOUT 10)

add_executable(kick-bar-seek-tests BarSeekTests.cpp)
add_test(NAME kick-bar-seek COMMAND kick-bar-seek-tests)
set_tests_properties(kick-bar-seek PROPERTIES TIMEOUT 10)

add_executable(kick-play-start-tests PlayStartTests.cpp)
add_test(NAME kick-play-start COMMAND kick-play-start-tests)
set_tests_properties(kick-play-start PROPERTIES TIMEOUT 10)

add_executable(kick-pad-colour-tests PadColourMapTests.cpp)
add_test(NAME kick-pad-colour COMMAND kick-pad-colour-tests "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/Default/theme.xml")
set_tests_properties(kick-pad-colour PROPERTIES TIMEOUT 10)

add_executable(kick-keyboard-tests KeyboardDetectorTests.cpp)
target_include_directories(kick-keyboard-tests PRIVATE ../../modules ../../modules/3rdParty ../../../ComProtocol)
target_link_libraries(kick-keyboard-tests PRIVATE contracts ipc juce::juce_gui_basics)
if(APPLE)
    target_link_libraries(kick-keyboard-tests PRIVATE "-framework Carbon")
endif()
target_compile_definitions(kick-keyboard-tests PRIVATE KICK_DESKTOP_KEYBOARD=1 KICK_KEYBOARD_PANEL=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
add_test(NAME kick-keyboard COMMAND kick-keyboard-tests)
set_tests_properties(kick-keyboard PROPERTIES TIMEOUT 10)

add_executable(kick-arpeggiator-tests ArpeggiatorTests.cpp)
add_test(NAME kick-arpeggiator COMMAND kick-arpeggiator-tests)
set_tests_properties(kick-arpeggiator PROPERTIES TIMEOUT 10)

add_library(kick-mock-clap-synth MODULE MockClapSynth.cpp)
target_include_directories(kick-mock-clap-synth PRIVATE ../../clap_host/third_party/clap/include)
set_target_properties(kick-mock-clap-synth PROPERTIES PREFIX "" SUFFIX ".clap")

add_library(kick-mock-clap-effect MODULE MockClapEffect.cpp)
target_include_directories(kick-mock-clap-effect PRIVATE ../../clap_host/third_party/clap/include)
set_target_properties(kick-mock-clap-effect PROPERTIES PREFIX "" SUFFIX ".clap")

add_executable(kick-clap-effect-tests
    ClapEffectHostTests.cpp
)
target_include_directories(kick-clap-effect-tests PRIVATE
    ../../clap_host/third_party/clap/include
    ../../clap_host/include
    ../../modules
    ../../../AudIO
    ../../../AudIO/AudIO
    ../../../AudIO/Includes
)
target_link_libraries(kick-clap-effect-tests PRIVATE kick_clap_host kick_audio_core ${CMAKE_DL_LIBS})
add_dependencies(kick-clap-effect-tests kick-mock-clap-effect)
add_test(NAME kick-clap-effect COMMAND kick-clap-effect-tests $<TARGET_FILE:kick-mock-clap-effect>)
set_tests_properties(kick-clap-effect PROPERTIES TIMEOUT 10)

add_executable(kick-plugin-manager-tests
    PluginManagerTests.cpp
)
target_include_directories(kick-plugin-manager-tests PRIVATE
    ../../clap_host/include
    ../../clap_host/third_party/clap/include
    ../../modules/3rdParty/tweetnacl
)
target_link_libraries(kick-plugin-manager-tests PRIVATE kick_clap_host ${CMAKE_DL_LIBS})
add_test(NAME kick-plugin-manager COMMAND kick-plugin-manager-tests)
set_tests_properties(kick-plugin-manager PROPERTIES TIMEOUT 10)

add_executable(kick-clap-processor-tests
    ClapInstrumentProcessorTests.cpp
    ../../../AudIO/processors/ClapInstrumentProcessor.cpp
)
target_include_directories(kick-clap-processor-tests PRIVATE
    ../../../AudIO/processors
    ../../clap_host/third_party/clap/include
)
target_link_libraries(kick-clap-processor-tests PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(kick-clap-processor-tests kick-mock-clap-synth)
add_test(NAME kick-clap-processor COMMAND kick-clap-processor-tests $<TARGET_FILE:kick-mock-clap-synth>)
set_tests_properties(kick-clap-processor PROPERTIES TIMEOUT 10)

if(NOT TARGET AudIO_juce)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../../AudIO/juce_modules.cmake")
endif()
add_executable(kick-audio-platform-tests AudioTests.cpp)
target_link_libraries(kick-audio-platform-tests PRIVATE AudIO_juce)
target_compile_definitions(kick-audio-platform-tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
add_test(NAME kick-audio-platform COMMAND kick-audio-platform-tests)
set_tests_properties(kick-audio-platform PROPERTIES TIMEOUT 20)

add_executable(kick-key-transpose-tests KeyTransposeTests.cpp)
add_test(NAME kick-key-transpose COMMAND kick-key-transpose-tests)
set_tests_properties(kick-key-transpose PROPERTIES TIMEOUT 10)
if(TARGET Kick)
    # The in-app smoke hooks (KICK_SMOKE_* env, AppSmoke.h) exist whenever the platform tests are built; the CLAP fixture
    # is only needed by the Python supervisor tests below.
    target_compile_definitions(Kick PRIVATE KICK_APP_SMOKE_TEST=1)
endif()
if(TARGET Kick AND TARGET kick-clap-fixture-normal)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_dependencies(Kick kick-clap-fixture-normal)
    add_test(NAME kick-app-smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/AppSmoke.py"
        $<TARGET_FILE:Kick> "${CMAKE_BINARY_DIR}" $<TARGET_FILE:kick-clap-fixture-normal>)
    set_tests_properties(kick-app-smoke PROPERTIES TIMEOUT 55)
    add_test(NAME kick-app-startup COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/AppSmoke.py"
        $<TARGET_FILE:Kick> "${CMAKE_BINARY_DIR}" $<TARGET_FILE:kick-clap-fixture-normal> --startup-only)
    set_tests_properties(kick-app-startup PROPERTIES TIMEOUT 55)
endif()
# Timing correct (note quantize) logic; JUCE-free, header only.
add_executable(kick-timing-correct-tests TimingCorrectTests.cpp)
add_test(NAME kick-timing-correct COMMAND kick-timing-correct-tests)
# Step sequencer pure logic (pages, step mapping, value ranges, nudge wrap, TIME DIV list); JUCE-free, header only.
add_executable(kick-step-model-tests StepModelTests.cpp)
add_test(NAME kick-step-model COMMAND kick-step-model-tests)
set_tests_properties(kick-step-model PROPERTIES TIMEOUT 10)
# Next Sequence / Song / Color Edit / List Edit pure logic; JUCE-free, header only.
add_executable(kick-sequence-logic-tests SequenceLogicTests.cpp)
add_test(NAME kick-sequence-logic COMMAND kick-sequence-logic-tests)
set_tests_properties(kick-sequence-logic PROPERTIES TIMEOUT 10)
add_executable(kick-qlink-logic-tests QLinkLogicTests.cpp)
add_test(NAME kick-qlink-logic COMMAND kick-qlink-logic-tests)
set_tests_properties(kick-qlink-logic PROPERTIES TIMEOUT 10)
add_executable(kick-midi-control-logic-tests MidiControlLogicTests.cpp)
add_test(NAME kick-midi-control-logic COMMAND kick-midi-control-logic-tests)
set_tests_properties(kick-midi-control-logic PROPERTIES TIMEOUT 10)
add_executable(kick-looper-logic-tests LooperLogicTests.cpp)
add_test(NAME kick-looper-logic COMMAND kick-looper-logic-tests)
set_tests_properties(kick-looper-logic PROPERTIES TIMEOUT 10)
add_executable(kick-list-edit-logic-tests ListEditLogicTests.cpp)
add_test(NAME kick-list-edit-logic COMMAND kick-list-edit-logic-tests)
set_tests_properties(kick-list-edit-logic PROPERTIES TIMEOUT 10)
add_executable(kick-pad-color-logic-tests PadColorLogicTests.cpp)
add_test(NAME kick-pad-color-logic COMMAND kick-pad-color-logic-tests)
set_tests_properties(kick-pad-color-logic PROPERTIES TIMEOUT 10)
# Button LED helpers (note map, levels, write cache); JUCE-free, header only.
add_executable(kick-led-map-tests LedMapTests.cpp)
add_test(NAME kick-led-map COMMAND kick-led-map-tests)
set_tests_properties(kick-led-map PROPERTIES TIMEOUT 10)
# Akai MPC Key 61 note tables (button + pad); ALSA/JUCE-free, header only.
add_executable(kick-akai-map-tests AkaiMapTests.cpp)
add_test(NAME kick-akai-map COMMAND kick-akai-map-tests)
set_tests_properties(kick-akai-map PROPERTIES TIMEOUT 10)
# Device discovery text parsing (cards, touch name, ranges, fallbacks); ALSA/JUCE-free, header only.
add_executable(kick-device-probe-tests DeviceProbeTests.cpp)
add_test(NAME kick-device-probe COMMAND kick-device-probe-tests)
set_tests_properties(kick-device-probe PROPERTIES TIMEOUT 10)
# LOCATE + < / > event selection (previous/next note start); JUCE-free, header only.
add_executable(kick-event-seek-tests EventSeekTests.cpp)
add_test(NAME kick-event-seek COMMAND kick-event-seek-tests)
set_tests_properties(kick-event-seek PROPERTIES TIMEOUT 10)
# Touch strip mode maths (8 modes: slider, pitch bend, pad level, mod wheel, track level, sustain, expression, off),
# Pad Level note mapping and the strip LED levels; JUCE-free, header only.
add_executable(kick-strip-modes-tests StripModesTests.cpp)
add_test(NAME kick-strip-modes COMMAND kick-strip-modes-tests)
set_tests_properties(kick-strip-modes PROPERTIES TIMEOUT 10)
# Read-only system probe report (formatter + fake root dir); JUCE/ALSA-free, header only.
add_executable(kick-system-probe-tests SystemProbeTests.cpp)
add_test(NAME kick-system-probe COMMAND kick-system-probe-tests)
set_tests_properties(kick-system-probe PROPERTIES TIMEOUT 10)

# Automation lanes (breakpoints, interpolation, record/overwrite, thinning, text form); JUCE-free, header only.
add_executable(kick-automation-tests AutomationTests.cpp)
add_test(NAME kick-automation COMMAND kick-automation-tests)
set_tests_properties(kick-automation PROPERTIES TIMEOUT 10)

# Program pan -> MIDI CC10 value mapping; JUCE-free, header only.
add_executable(kick-midi-pan-tests MidiPanTests.cpp)
add_test(NAME kick-midi-pan COMMAND kick-midi-pan-tests)
set_tests_properties(kick-midi-pan PROPERTIES TIMEOUT 10)

# MIDI Program logic: note/vel range, transpose, filters, learn, unique names, CC labels; JUCE-free, header only.
add_executable(kick-midi-program-tests MidiProgramTests.cpp)
add_test(NAME kick-midi-program COMMAND kick-midi-program-tests)
set_tests_properties(kick-midi-program PROPERTIES TIMEOUT 10)

# CV step (0..127) -> 12 bit DAC scaling; JUCE-free, header only.
add_executable(kick-cv-scale-tests CvScaleTests.cpp)
add_test(NAME kick-cv-scale COMMAND kick-cv-scale-tests)
set_tests_properties(kick-cv-scale PROPERTIES TIMEOUT 10)

# Sustain pedal (CC64/CC123) state machine; JUCE-free, header only.
add_executable(kick-sustain-tests SustainTests.cpp)
add_test(NAME kick-sustain COMMAND kick-sustain-tests)
set_tests_properties(kick-sustain PROPERTIES TIMEOUT 10)

# Bridge helpers: two-Shift counter, note/bend/sustain target tables; JUCE-free, header only.
add_executable(kick-hw-targets-tests HwTargetsTests.cpp)
add_test(NAME kick-hw-targets COMMAND kick-hw-targets-tests)
set_tests_properties(kick-hw-targets PROPERTIES TIMEOUT 10)

# Muted track: which live messages still pass (note-off, pitch wheel, CC64, CC123); JUCE-free, header only.
add_executable(kick-mute-filter-tests MuteFilterTests.cpp)
add_test(NAME kick-mute-filter COMMAND kick-mute-filter-tests)
set_tests_properties(kick-mute-filter PROPERTIES TIMEOUT 10)
# Track Monitor (Off / In / Auto): which live MIDI input reaches the program; JUCE-free, header only.
add_executable(kick-track-monitor-tests TrackMonitorTests.cpp)
add_test(NAME kick-track-monitor COMMAND kick-track-monitor-tests)
set_tests_properties(kick-track-monitor PROPERTIES TIMEOUT 10)
# Sequence time signature change: bar count / loop / track length re-mapping; JUCE-free, header only.
add_executable(kick-time-signature-tests TimeSignatureTests.cpp)
add_test(NAME kick-time-signature COMMAND kick-time-signature-tests)
set_tests_properties(kick-time-signature PROPERTIES TIMEOUT 10)
# Engine list vs ValueTree reconcile (undo/redo of sequence/track/program create+delete); JUCE-free, header only.
add_executable(kick-tree-sync-tests TreeSyncTests.cpp)
add_test(NAME kick-tree-sync COMMAND kick-tree-sync-tests)
set_tests_properties(kick-tree-sync PROPERTIES TIMEOUT 10)
# Touch coordinate chain raw -> framebuffer 1280x800 -> UI 1024x600; JUCE-free, header only.
add_executable(kick-touch-space-tests TouchSpaceTests.cpp)
add_test(NAME kick-touch-space COMMAND kick-touch-space-tests)
set_tests_properties(kick-touch-space PROPERTIES TIMEOUT 10)

# Hardware keys that close the modal Timing Correct dialog (ENTER/STOP apply, Shift+STOP/MAIN cancel); JUCE-free, header only.
add_executable(kick-modal-dialog-keys-tests ModalDialogKeysTests.cpp)
add_test(NAME kick-modal-dialog-keys COMMAND kick-modal-dialog-keys-tests)
set_tests_properties(kick-modal-dialog-keys PROPERTIES TIMEOUT 10)

# Sequence recording (REC/OVERDUB + PLAY): arm/record state machine, erase rule, capture buffer + pairing; JUCE-free, header only.
add_executable(kick-seq-record-tests SeqRecordTests.cpp)
add_test(NAME kick-seq-record COMMAND kick-seq-record-tests)
set_tests_properties(kick-seq-record PROPERTIES TIMEOUT 20)

# Akai Main Volume SysEx (F0 47 7F 4B 5B 00 01 <v> F7) decoder + MidiParser SysEx assembly; JUCE-free, header only.
add_executable(kick-main-volume-tests MainVolumeTests.cpp)
add_test(NAME kick-main-volume COMMAND kick-main-volume-tests)
set_tests_properties(kick-main-volume PROPERTIES TIMEOUT 10)

# Full Level / Half Level pad velocity rules (MPC v3.10 user guide p. 343); JUCE-free, header only.
add_executable(kick-pad-level-tests PadLevelTests.cpp)
add_test(NAME kick-pad-level COMMAND kick-pad-level-tests)
set_tests_properties(kick-pad-level PROPERTIES TIMEOUT 10)

# Relative controller decoding (Data Dial CC100 0x01/0x7F, Q-Links); JUCE-free, header only.
add_executable(kick-relative-cc-tests RelativeCcTests.cpp)
add_test(NAME kick-relative-cc COMMAND kick-relative-cc-tests)
set_tests_properties(kick-relative-cc PROPERTIES TIMEOUT 10)

# AG1b: two-thread stress test of the bridge's strip + pedal Sustain state (kick_hw::SustainState); also run it with -fsanitize=thread.
find_package(Threads REQUIRED)
add_executable(kick-sustain-stress-tests SustainStressTests.cpp)
target_link_libraries(kick-sustain-stress-tests PRIVATE Threads::Threads)
add_test(NAME kick-sustain-stress COMMAND kick-sustain-stress-tests)
set_tests_properties(kick-sustain-stress PROPERTIES TIMEOUT 60)

# Logging system: timestamp, thread ID, category filter, crash ring buffer, log rotation; JUCE-free, header only.
add_executable(kick-logging-tests LoggingTests.cpp)
add_test(NAME kick-logging COMMAND kick-logging-tests)
set_tests_properties(kick-logging PROPERTIES TIMEOUT 10)

# Preferences & Platform command helpers (sanitization, injection protection, date/time formatting, CV calibration math, VERSION parsing); JUCE-free, header only.
add_executable(kick-platform-command-tests PlatformCommandTests.cpp)
add_test(NAME kick-platform-command COMMAND kick-platform-command-tests)
set_tests_properties(kick-platform-command PROPERTIES TIMEOUT 10)

# MPC hidden top-level window: reallyContains() is false, contains() true (why KeyboardComponent::xyToNote uses contains()).
add_executable(kick-hidden-window-hit-tests HiddenWindowHitTestTests.cpp)
target_link_libraries(kick-hidden-window-hit-tests PRIVATE juce::juce_gui_basics)
target_compile_definitions(kick-hidden-window-hit-tests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
if(APPLE)
    target_link_libraries(kick-hidden-window-hit-tests PRIVATE "-framework Carbon")
endif()
add_test(NAME kick-hidden-window-hit COMMAND kick-hidden-window-hit-tests)
set_tests_properties(kick-hidden-window-hit PROPERTIES TIMEOUT 10)

# Touch piano pure logic (W3-A): multi-finger state machine, glissando note-offs, velocity modes, bend mapping, sustain held notes; JUCE-free, header only.
add_executable(kick-piano-logic-tests PianoLogicTests.cpp)
add_test(NAME kick-piano-logic COMMAND kick-piano-logic-tests)
set_tests_properties(kick-piano-logic PROPERTIES TIMEOUT 10)

# Pad Perform pure logic (S-F): pad layout (Chromatic / Notes / Start on Root), chord formulas, diatonic Progression, held-note table, 16 LEVELS, chord name; JUCE-free, header only.
add_executable(kick-pad-mapper-tests PadMapperTests.cpp)
add_test(NAME kick-pad-mapper COMMAND kick-pad-mapper-tests)
set_tests_properties(kick-pad-mapper PROPERTIES TIMEOUT 10)

# Layer Play selection (S-F): Cycle wraps, Random picks only layers with a sound, Velocity range closed (127 hit); JUCE-free, header only.
add_executable(kick-layer-play-tests LayerPlayTests.cpp)
add_test(NAME kick-layer-play COMMAND kick-layer-play-tests)
set_tests_properties(kick-layer-play PROPERTIES TIMEOUT 10)

# Audio editor SPLIT geometry (S-F); JUCE-free, header only.
add_executable(kick-audio-split-tests AudioSplitTests.cpp)
add_test(NAME kick-audio-split COMMAND kick-audio-split-tests)
set_tests_properties(kick-audio-split PROPERTIES TIMEOUT 10)

# Grid pure logic: pad-to-note bank mapping, button state machine, Q-Link zoom scaling; JUCE-free, header only.
add_executable(kick-grid-logic-tests GridLogicTests.cpp)
add_test(NAME kick-grid-logic COMMAND kick-grid-logic-tests)
set_tests_properties(kick-grid-logic PROPERTIES TIMEOUT 10)

# File-browser sources (M0/M1): WebDAV helpers (PROPFIND parser, URLs, Range/resume, cache invalidation), cache/LRU, secret files, config, local source, download queue; JUCE-free, header only.
add_executable(kick-webdav-helper-tests WebDavHelperTests.cpp)
add_test(NAME kick-webdav-helper COMMAND kick-webdav-helper-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
set_tests_properties(kick-webdav-helper PROPERTIES TIMEOUT 30)
add_executable(kick-source-cache-tests SourceCacheTests.cpp)
target_link_libraries(kick-source-cache-tests PRIVATE Threads::Threads)
add_test(NAME kick-source-cache COMMAND kick-source-cache-tests)
set_tests_properties(kick-source-cache PROPERTIES TIMEOUT 60)

# WebDavSource integration test against data/webdav_test_server.py (python3, 127.0.0.1); needs JUCE (juce_core) only.
add_executable(kick-webdav-source-tests WebDavSourceTests.cpp)
target_include_directories(kick-webdav-source-tests PRIVATE ../../modules)
target_link_libraries(kick-webdav-source-tests PRIVATE juce::juce_core)
target_compile_definitions(kick-webdav-source-tests PRIVATE JUCE_USE_CURL=0)
add_test(NAME kick-webdav-source COMMAND kick-webdav-source-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
set_tests_properties(kick-webdav-source PROPERTIES TIMEOUT 60)

if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()

# Startup: state machine + messages, safe-mode boot guard, status strip data, project load queue; JUCE-free, header only.
add_executable(kick-startup-tests StartupTests.cpp)
add_test(NAME kick-startup COMMAND kick-startup-tests)
set_tests_properties(kick-startup PROPERTIES TIMEOUT 20)

# Sample Assign pure logic: bank/pad index maths, pool mapping, Keygroup zones, audition debounce; JUCE-free, header only.
add_executable(kick-assign-sample-logic-tests AssignSampleLogicTests.cpp)
add_test(NAME kick-assign-sample-logic COMMAND kick-assign-sample-logic-tests)
set_tests_properties(kick-assign-sample-logic PROPERTIES TIMEOUT 10)

# Sampler pure logic: slice maths in the real sample rate, time-ordered pad slices, collision-free file names; JUCE-free, header only.
add_executable(kick-sampler-logic-tests SamplerLogicTests.cpp)
add_test(NAME kick-sampler-logic COMMAND kick-sampler-logic-tests)
set_tests_properties(kick-sampler-logic PROPERTIES TIMEOUT 10)

# Sample editor DSP helpers: fade curves, normalise gain, stereo->mono, edit-format guard, result file names; JUCE-free, header only.
add_executable(kick-sample-editor-actions-tests SampleEditorActionsTests.cpp)
add_test(NAME kick-sample-editor-actions COMMAND kick-sample-editor-actions-tests)
set_tests_properties(kick-sample-editor-actions PROPERTIES TIMEOUT 10)
