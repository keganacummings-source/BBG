cmake_minimum_required(VERSION 3.22)
project(BabyGirl VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BG_BUILD_TESTS "Build the pure-C++ DSP self-test (every machine, synth, drums, LUFS)" ON)
option(BG_COPY_AFTER_BUILD "Copy plugins into the system plugin folder after building" OFF)
option(BG_BUILD_PLUGIN "Build the JUCE plugins (needs network access to fetch JUCE)" ON)

# ---------------------------------------------------------------------------
# DSP self-test: no JUCE needed. `ctest` runs every machine at 3 sample rates.
# ---------------------------------------------------------------------------
if(BG_BUILD_TESTS)
    enable_testing()
    add_executable(bg_dsp_selftest tests/dsp_selftest.cpp)
    target_include_directories(bg_dsp_selftest PRIVATE Source/Core)
    if(MSVC)
        target_compile_options(bg_dsp_selftest PRIVATE /utf-8 /O2)
    else()
        target_compile_options(bg_dsp_selftest PRIVATE -O2)
    endif()
    add_test(NAME bg_dsp_selftest COMMAND bg_dsp_selftest)
endif()

if(NOT BG_BUILD_PLUGIN)
    return()
endif()

# Universal binary on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(BG_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/EditorPages.h
    Source/Visualizers.h
    Source/Params.h
    Source/HushDsp.h
    Source/KyotoDsp.h
    Source/MasterDsp.h
    Source/ModulesDsp.h
    Source/DreamShare.h
    Source/DreamApi.h
    Source/Presets.h
    Source/CustomLookAndFeel.h
    Source/Core/bg_common.h
    Source/Core/bg_machine.h
    Source/Core/fam_dyn_eq.h
    Source/Core/fam_flt_dst.h
    Source/Core/fam_dly_rev.h
    Source/Core/fam_mod_pit_spa.h
    Source/Core/fam_gim_forge.h
    Source/Core/bg_synth.h
    Source/Core/bg_drums.h
    Source/Core/bg_analysis.h
)

# Two products from one code base:
#   BabyGirl   - instrument (MIDI in, synth + drums + full FX rack)
#   BabyGirlFX - effect     (audio in, same engine, loads in any DAW effect slot)
function(bg_add_plugin TARGET IS_SYNTH PLUGIN_CODE IS_FX CATEGORY)
    juce_add_plugin(${TARGET}
        COMPANY_NAME "KeganaCummings"
        IS_SYNTH ${IS_SYNTH}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        PLUGIN_MANUFACTURER_CODE Kcum
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS VST3 Standalone
        VST3_CATEGORIES ${CATEGORY}
        PRODUCT_NAME "${TARGET}"
        COPY_PLUGIN_AFTER_BUILD ${BG_COPY_AFTER_BUILD}
        DESCRIPTION "BabyGirl - synth, drums and a 134-machine modular FX rack"
    )
    target_sources(${TARGET} PRIVATE ${BG_SOURCES})
    target_include_directories(${TARGET} PRIVATE Source Source/Core)
    target_link_libraries(${TARGET} PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
    )
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /utf-8 /MP /bigobj)
    endif()
    # DreamAPI needs HTTP. Windows/macOS use the OS stack; Linux uses libcurl (installed in CI).
    if(UNIX AND NOT APPLE)
        set(BG_CURL 1)
    else()
        set(BG_CURL 0)
    endif()
    target_compile_definitions(${TARGET} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=${BG_CURL}
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        BG_FX_BUILD=${IS_FX}
        BG_PRODUCT_NAME="${TARGET}"
        BABYGIRL_VERSION="${PROJECT_VERSION}"
    )
endfunction()

bg_add_plugin(BabyGirl   TRUE  BbyG 0 "Instrument Synth")
bg_add_plugin(BabyGirlFX FALSE BbyF 1 "Fx")
