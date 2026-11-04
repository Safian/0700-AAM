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
enable_testing()
add_executable(kick-serial-tests SerialTests.cpp ../../modules/serial_port/serial_port.cpp ../../../ComProtocol/PktMgr/PktMgr.cpp)
target_include_directories(kick-serial-tests PRIVATE ../../modules ../../../ComProtocol)
target_link_libraries(kick-serial-tests PRIVATE juce::juce_core)
target_compile_definitions(kick-serial-tests PRIVATE JUCE_USE_CURL=0)
add_test(NAME kick-serial COMMAND kick-serial-tests)
set_tests_properties(kick-serial PROPERTIES TIMEOUT 10)

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
if(TARGET Kick AND TARGET kick-clap-fixture-normal)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    target_compile_definitions(Kick PRIVATE KICK_APP_SMOKE_TEST=1)
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

# Grid pure logic: pad-to-note bank mapping, button state machine, Q-Link zoom scaling; JUCE-free, header only.
add_executable(kick-grid-logic-tests GridLogicTests.cpp)
add_test(NAME kick-grid-logic COMMAND kick-grid-logic-tests)
set_tests_properties(kick-grid-logic PROPERTIES TIMEOUT 10)

if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()
