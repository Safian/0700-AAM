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

add_executable(kick-keyboard-tests KeyboardDetectorTests.cpp)
target_include_directories(kick-keyboard-tests PRIVATE ../../modules ../../modules/3rdParty ../../../ComProtocol)
target_link_libraries(kick-keyboard-tests PRIVATE contracts ipc juce::juce_gui_basics "-framework Carbon")
target_compile_definitions(kick-keyboard-tests PRIVATE KICK_DESKTOP_KEYBOARD=1 KICK_KEYBOARD_PANEL=1)
add_test(NAME kick-keyboard COMMAND kick-keyboard-tests)
set_tests_properties(kick-keyboard PROPERTIES TIMEOUT 10)

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
if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()
