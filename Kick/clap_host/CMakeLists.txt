cmake_minimum_required(VERSION 3.16)
project(KickClapHost VERSION 0.1.0 LANGUAGES C CXX)

option(KICK_CLAP_BUILD_TESTS "Build CLAP host conformance fixtures" ON)
option(KICK_CLAP_SANITIZE "Enable address and undefined-behavior sanitizers" OFF)
option(KICK_CLAP_BUILD_JUCE "Build the reusable JUCE session and touchscreen panel" OFF)
option(KICK_CLAP_BUILD_DEVICE_BENCH "Build the audio-device measurement tool without the panel/player" OFF)
option(KICK_CLAP_BUILD_PLAYER "Build a standalone CLAP instrument host" OFF)
set(KICK_CLAP_JUCE_PATH "" CACHE PATH "Local JUCE source for standalone adapter builds")
add_library(synthyboy_shapes STATIC
    builtin/synthyboy/shapes/src/shape.cpp
    builtin/synthyboy/shapes/src/lfo.cpp
    builtin/synthyboy/shapes/src/envelope.cpp
    builtin/synthyboy/shapes/src/remap.cpp
)
target_include_directories(synthyboy_shapes
    PUBLIC builtin/synthyboy/shapes/include
    PRIVATE builtin/synthyboy/shapes/src
)

add_library(synthyboy_sampleosc STATIC
    builtin/synthyboy/sample-osc/src/sample_data.cpp
    builtin/synthyboy/sample-osc/src/sample_voice.cpp
    builtin/synthyboy/sample-osc/src/wav_reader.cpp
)
target_include_directories(synthyboy_sampleosc
    PUBLIC builtin/synthyboy/sample-osc/include
    PRIVATE builtin/synthyboy/sample-osc/src
)

add_library(synthyboy_granular STATIC builtin/synthyboy/granular/src/granular.cpp)
target_include_directories(synthyboy_granular PUBLIC builtin/synthyboy/granular/include)
target_link_libraries(synthyboy_granular PUBLIC synthyboy_sampleosc)

add_library(synthyboy_spectral STATIC builtin/synthyboy/spectral/src/spectral.cpp)
target_include_directories(synthyboy_spectral PUBLIC builtin/synthyboy/spectral/include)
target_link_libraries(synthyboy_spectral PUBLIC synthyboy_sampleosc)

add_library(synthyboy_multisample STATIC builtin/synthyboy/multisample/src/multisample.cpp)
target_include_directories(synthyboy_multisample PUBLIC builtin/synthyboy/multisample/include)
target_link_libraries(synthyboy_multisample PUBLIC synthyboy_sampleosc)

add_library(synthyboy_filters_serum STATIC
    builtin/synthyboy/filters-serum/src/filter.cpp
    builtin/synthyboy/filters-serum/src/filter_types.cpp
)
target_include_directories(synthyboy_filters_serum
    PUBLIC builtin/synthyboy/filters-serum/include
    PRIVATE builtin/synthyboy/filters-serum/src
)

add_library(synthyboy_wtedit STATIC
    builtin/synthyboy/wavetable-edit/src/fft.cpp
    builtin/synthyboy/wavetable-edit/src/wavetable.cpp
    builtin/synthyboy/wavetable-edit/src/drawing_tools.cpp
    builtin/synthyboy/wavetable-edit/src/frame_ops.cpp
    builtin/synthyboy/wavetable-edit/src/vital_model.cpp
    builtin/synthyboy/wavetable-edit/src/undo.cpp
    builtin/synthyboy/wavetable-edit/src/audio_import.cpp
    builtin/synthyboy/wavetable-edit/src/image_import.cpp
    builtin/synthyboy/wavetable-edit/src/wav_export.cpp
    builtin/synthyboy/wavetable-edit/src/player.cpp
)
target_include_directories(synthyboy_wtedit
    PUBLIC builtin/synthyboy/include
    PRIVATE builtin/synthyboy/wavetable-edit/src
)

add_library(kick_clap_sanitizers INTERFACE)
if(KICK_CLAP_SANITIZE AND NOT MSVC)
    target_compile_options(kick_clap_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(kick_clap_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

add_library(synthyboy_core STATIC
    builtin/synthyboy/src/content_pack.cpp
    builtin/synthyboy/src/content_state.cpp
    builtin/synthyboy/src/content_store.cpp
    builtin/synthyboy/src/engine_caps.cpp
    builtin/synthyboy/src/engine_factory.cpp
    builtin/synthyboy/src/fx_rack.cpp
    builtin/synthyboy/src/fx_types.cpp
    builtin/synthyboy/src/kick_pages.cpp
    builtin/synthyboy/src/library_items.cpp
    builtin/synthyboy/src/modulation.cpp
    builtin/synthyboy/src/number.cpp
    builtin/synthyboy/src/offline_render.cpp
    builtin/synthyboy/src/param_table.cpp
    builtin/synthyboy/src/params.cpp
    builtin/synthyboy/src/preferences.cpp
    builtin/synthyboy/src/preset.cpp
    builtin/synthyboy/src/sha256.cpp
    builtin/synthyboy/src/shell_features.cpp
    builtin/synthyboy/src/state.cpp
    builtin/synthyboy/src/user_content.cpp
    builtin/synthyboy/src/user_library.cpp
    builtin/synthyboy/src/wave_convert.cpp
    builtin/synthyboy/engine-own/own_engine.cpp
    builtin/synthyboy/formula/src/formula.cpp
    builtin/synthyboy/tuning/tuning.cpp
    builtin/synthyboy/tuning/scl_parser.cpp
    builtin/synthyboy/tuning/kbm_parser.cpp
    builtin/synthyboy/tuning/tun_parser.cpp
    builtin/synthyboy/wavetable-render/wavetable_render.cpp
    builtin/synthyboy/vital-import/zip_reader.cpp
    builtin/synthyboy/vital-import/json_reader.cpp
    builtin/synthyboy/vital-import/vital_map.cpp
    builtin/synthyboy/vital-import/vital_content.cpp
    builtin/synthyboy/vital-import/builtin_ref.cpp
    builtin/synthyboy/vital-import/vital_batch.cpp
    builtin/synthyboy/vital-import/plugin_import.cpp
    builtin/synthyboy/vital-import/vital_import.cpp
    builtin/synthyboy/serum-import/zstd_decode.cpp
    builtin/synthyboy/serum-import/cbor_reader.cpp
    builtin/synthyboy/serum-import/audio_decode.cpp
    builtin/synthyboy/serum-import/serum_container.cpp
    builtin/synthyboy/serum-import/serum_map.cpp
    builtin/synthyboy/serum-import/serum_import.cpp
    builtin/synthyboy/serum-import/serum_voice.cpp
    builtin/synthyboy/serum-import/serum_content.cpp
    builtin/synthyboy/serum-import/serum_fx.cpp
    builtin/synthyboy/serum-import/serum_api.cpp
    builtin/synthyboy/serum-import/serum_plugin_import.cpp
    builtin/synthyboy/synthyboy_clap.cpp
    builtin/synthyboy/synthyboy_content_service.cpp
)
target_include_directories(synthyboy_core
    PUBLIC
        builtin/synthyboy/include
        third_party/clap/include
    PRIVATE
        builtin/synthyboy
        builtin/synthyboy/src
        builtin/synthyboy/engine-own
        builtin/synthyboy/formula/src
        builtin/synthyboy/tuning
        builtin/synthyboy/vital-import
        builtin/synthyboy/serum-import
)
target_compile_definitions(synthyboy_core PRIVATE
    SYNTHYBOY_HAS_VITAL=0
    SYNTHYBOY_STATIC_ONLY=1
)
target_link_libraries(synthyboy_core PRIVATE
    synthyboy_shapes
    synthyboy_sampleosc
    synthyboy_granular
    synthyboy_spectral
    synthyboy_multisample
    synthyboy_filters_serum
    synthyboy_wtedit
)

foreach(sb_lib IN ITEMS
    synthyboy_shapes synthyboy_sampleosc synthyboy_granular
    synthyboy_spectral synthyboy_multisample synthyboy_filters_serum synthyboy_wtedit synthyboy_core)
    target_compile_features(${sb_lib} PUBLIC cxx_std_17)
    set_target_properties(${sb_lib} PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
    target_link_libraries(${sb_lib} PRIVATE kick_clap_sanitizers)
    if(MSVC)
        target_compile_options(${sb_lib} PRIVATE /W4 /fp:precise)
    else()
        target_compile_options(${sb_lib} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math)
    endif()
endforeach()

add_library(kick_clap_host STATIC
    src/Host.cpp
    src/Discovery.cpp
    src/PluginManifest.cpp
    src/TrustStore.cpp
    src/PluginInstaller.cpp
    src/PluginLoader.cpp
    src/PluginManager.cpp
    ../modules/3rdParty/tweetnacl/tweetnacl.c
    ../modules/3rdParty/tweetnacl/sha256.c
    ../modules/3rdParty/tweetnacl/crypto_utils.cpp
)
target_compile_features(kick_clap_host PUBLIC cxx_std_17)
set_target_properties(kick_clap_host PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kick_clap_host PUBLIC
    include
    third_party/clap/include
    ../modules/3rdParty/tweetnacl
    builtin/synthyboy/include
)
target_link_libraries(kick_clap_host PRIVATE synthyboy_core)
target_link_libraries(kick_clap_host PUBLIC ${CMAKE_DL_LIBS})
find_package(Threads REQUIRED)
target_link_libraries(kick_clap_host PUBLIC Threads::Threads kick_clap_sanitizers)
if(MSVC)
    target_compile_options(kick_clap_host PRIVATE /W4 /fp:precise)
else()
    target_compile_options(kick_clap_host PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(kick-clap-scan tools/scan.cpp)
    target_link_libraries(kick-clap-scan PRIVATE kick_clap_host)
    install(TARGETS kick-clap-scan RUNTIME DESTINATION bin BUNDLE DESTINATION bin)
endif()
# Measurement tools use the same core as KICK and are installable on the board.
add_library(kick_clap_benchmark STATIC tools/Benchmark.cpp)
target_include_directories(kick_clap_benchmark PUBLIC tools)
target_link_libraries(kick_clap_benchmark PUBLIC kick_clap_host)
if(NOT MSVC)
    target_compile_options(kick_clap_benchmark PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math)
endif()
set(KICK_BENCH_REVISION "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE KICK_BENCH_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE KICK_BENCH_GIT_RESULT ERROR_QUIET)
    if(KICK_BENCH_GIT_RESULT EQUAL 0)
        set(KICK_BENCH_REVISION "${KICK_BENCH_GIT_REVISION}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE KICK_BENCH_DIRTY ERROR_QUIET)
        if(KICK_BENCH_DIRTY)
            string(APPEND KICK_BENCH_REVISION "+dirty")
        endif()
    endif()
endif()

target_compile_definitions(kick_clap_benchmark PRIVATE KICK_BENCH_REVISION="${KICK_BENCH_REVISION}" KICK_BENCH_BUILD_TYPE="$<CONFIG>")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(kick-clap-bench tools/bench.cpp)
    target_link_libraries(kick-clap-bench PRIVATE kick_clap_benchmark)
    install(TARGETS kick-clap-bench RUNTIME DESTINATION bin BUNDLE DESTINATION bin)
    install(PROGRAMS tools/benchmark_matrix.py DESTINATION bin RENAME kick-clap-benchmark-matrix)
endif()
function(kick_bundle_clap_scanner target)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_dependencies(${target} kick-clap-scan)
        add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:kick-clap-scan> $<TARGET_FILE_DIR:${target}>/kick-clap-scan)
    endif()
endfunction()
if(KICK_CLAP_BUILD_JUCE OR KICK_CLAP_BUILD_PLAYER OR KICK_CLAP_BUILD_DEVICE_BENCH)
    if(NOT TARGET juce::juce_audio_devices)
        if(NOT EXISTS "${KICK_CLAP_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR "Set KICK_CLAP_JUCE_PATH to a JUCE source checkout")
        endif()
        add_subdirectory("${KICK_CLAP_JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_executable(kick-clap-device-bench tools/device_bench.cpp)
        target_link_libraries(kick-clap-device-bench PRIVATE kick_clap_benchmark juce::juce_audio_devices)
        target_compile_definitions(kick-clap-device-bench PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
        install(TARGETS kick-clap-device-bench RUNTIME DESTINATION bin BUNDLE DESTINATION bin)
    endif()
    if(KICK_CLAP_BUILD_JUCE OR KICK_CLAP_BUILD_PLAYER)
        add_library(kick_clap_juce STATIC juce/PluginSession.cpp juce/PluginPanel.cpp)
        target_include_directories(kick_clap_juce PUBLIC juce)
        target_link_libraries(kick_clap_juce PUBLIC kick_clap_host juce::juce_audio_utils juce::juce_gui_extra)
        target_compile_definitions(kick_clap_juce PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        if(KICK_CLAP_BUILD_PLAYER)
            juce_add_gui_app(kick-clap-player PRODUCT_NAME "KICK CLAP Host")
            target_sources(kick-clap-player PRIVATE tools/Player.cpp)
            target_link_libraries(kick-clap-player PRIVATE kick_clap_juce)
            add_dependencies(kick-clap-player kick-clap-scan)
            add_custom_command(TARGET kick-clap-player POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE:kick-clap-scan> $<TARGET_FILE_DIR:kick-clap-player>/kick-clap-scan)
        endif()
    endif()
endif()
if(KICK_CLAP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
# JUCE's IDE-only module headers include internal .cpp files. Static libraries
# need the same per-directory source-property fixup that juce_add_gui_app uses.
if(COMMAND _juce_fixup_module_source_groups)
    _juce_fixup_module_source_groups()
endif()
