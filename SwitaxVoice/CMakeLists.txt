cmake_minimum_required(VERSION 3.22)

project(SwitaxVoice VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SWITAX_BUILD_TESTS "Build the core / plugin test executables" ON)
option(SWITAX_STANDALONE "Also build the Standalone application" ON)
set(SWITAX_JUCE_DIR "" CACHE PATH "Path to a JUCE 8 checkout (empty = download JUCE 8.0.4)")

# Shared VocalSweet DSP primitives (TPT SVF, compressor, de-esser, saturation, limiter...).
# They are used read-only from the VocalSweet tree so the known-working plugin is untouched.
set(VOCALSWEET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../VocalSweet/Source" CACHE PATH "VocalSweet Source directory (shared DSP)")
if(NOT EXISTS "${VOCALSWEET_SOURCE_DIR}/dsp/Svf.h")
    message(FATAL_ERROR "Shared DSP not found at ${VOCALSWEET_SOURCE_DIR}/dsp (set VOCALSWEET_SOURCE_DIR)")
endif()

# ---------------------------------------------------------------------------------------------
# JUCE
if(SWITAX_JUCE_DIR)
    add_subdirectory(${SWITAX_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------------------------
set(SX_FORMATS VST3)
if(SWITAX_STANDALONE)
    list(APPEND SX_FORMATS Standalone)
endif()

set(SX_AUTO_MANIFEST TRUE)
if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    set(SX_AUTO_MANIFEST FALSE)
endif()

juce_add_plugin(SwitaxVoice
    PRODUCT_NAME "SwitaxVoice"
    COMPANY_NAME "Switax Audio"
    COMPANY_WEBSITE "https://example.invalid/switaxvoice"
    BUNDLE_ID com.switax.switaxvoice
    PLUGIN_MANUFACTURER_CODE Swtx
    PLUGIN_CODE SxV1
    FORMATS ${SX_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Analyzer Dynamics EQ
    VST3_AUTO_MANIFEST ${SX_AUTO_MANIFEST}
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(SwitaxVoice)

set(SX_PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AnalysisEngine.cpp
    Source/PresetManager.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/AssistPage.cpp
    Source/ui/ChannelPage.cpp)

target_sources(SwitaxVoice PRIVATE ${SX_PLUGIN_SOURCES})

juce_add_binary_data(SwitaxVoiceBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Resources/fonts/SpaceGrotesk-Regular.ttf
        Resources/fonts/SpaceGrotesk-Medium.ttf
        Resources/fonts/SpaceGrotesk-Bold.ttf
        Resources/fonts/Inter-Regular.ttf
        Resources/fonts/Inter-SemiBold.ttf)

set(SX_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_compile_definitions(SwitaxVoice PUBLIC ${SX_DEFINITIONS})
target_include_directories(SwitaxVoice PUBLIC Source ${VOCALSWEET_SOURCE_DIR})

target_link_libraries(SwitaxVoice
    PRIVATE
        SwitaxVoiceBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------------------------
# Tests and tools (host builds only)
if(SWITAX_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    # Framework-independent tests: loudness/true-peak standards cases, spectrum, pitch, sibilance,
    # stereo, DSP chain, advisor behaviour on synthetic and (if present) real recordings.
    add_executable(SwitaxCoreTests tests/CoreTests.cpp)
    target_include_directories(SwitaxCoreTests PRIVATE Source tests ${VOCALSWEET_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../VocalSweet/tests)
    target_compile_options(SwitaxCoreTests PRIVATE -O2 -Wall -Wextra)
    add_test(NAME core COMMAND SwitaxCoreTests ${CMAKE_BINARY_DIR}/test-output)

    # Plugin-level tests on the real processor/editor classes (in-process host)
    juce_add_console_app(SwitaxPluginTests PRODUCT_NAME "SwitaxPluginTests")
    juce_generate_juce_header(SwitaxPluginTests)
    target_sources(SwitaxPluginTests PRIVATE tests/PluginTests.cpp ${SX_PLUGIN_SOURCES})
    target_include_directories(SwitaxPluginTests PRIVATE Source tests ${VOCALSWEET_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../VocalSweet/tests)
    target_compile_definitions(SwitaxPluginTests PRIVATE ${SX_DEFINITIONS}
        JucePlugin_Name="SwitaxVoice" JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(SwitaxPluginTests PRIVATE
        SwitaxVoiceBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    add_test(NAME plugin COMMAND SwitaxPluginTests ${CMAKE_BINARY_DIR}/test-output)
endif()
