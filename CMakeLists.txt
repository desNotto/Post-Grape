cmake_minimum_required(VERSION 3.22)

project(PostGrape VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime: the VST3 does not need the MSVC redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE 8: use the local copy in external/JUCE, or download it.
if(EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.17-security
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PostGrape
    COMPANY_NAME "desNotto"
    COMPANY_WEBSITE "https://github.com/desNotto/Post-Grape"
    PLUGIN_MANUFACTURER_CODE DsNt
    PLUGIN_CODE PGrp
    FORMATS VST3 Standalone
    PRODUCT_NAME "Post-Grape"
    DESCRIPTION "Guitar bus post-FX: EQ, low-band compressor, wider, mono-maker, true stereo pan, lofi, true-peak limiter, auto-gain"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(PostGrapeData
    HEADER_NAME PostGrapeData.h
    NAMESPACE PostGrapeData
    SOURCES
        assets/fonts/Nunito-Bold.ttf
        assets/fonts/Nunito-ExtraBold.ttf
        assets/fonts/Nunito-Black.ttf
        assets/images/grapes.png
        assets/images/leaf.png
        assets/images/leafsmall.png
        assets/images/swirl.png
        assets/images/vine.png)

target_sources(PostGrape PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Params.cpp
    src/PresetManager.cpp
    src/UpdateChecker.cpp
    src/gui/Hints.cpp
    src/gui/Theme.cpp
    src/gui/Widgets.cpp
    src/gui/Displays.cpp
    src/gui/Panels.cpp)

target_include_directories(PostGrape PRIVATE src)

# Source files are UTF-8 (Russian hint texts).
target_compile_options(PostGrape PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(PostGrape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(PostGrape
    PRIVATE
        PostGrapeData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Copy the built VST3 bundle to <project>/bin.
add_custom_command(TARGET PostGrape_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_SOURCE_DIR}/bin/Post-Grape.vst3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:PostGrape_VST3>/../.."
            "${CMAKE_SOURCE_DIR}/bin/Post-Grape.vst3"
    COMMENT "Copy Post-Grape.vst3 to bin/")

# DSP tests (no JUCE).
add_executable(PostGrapeTests tests/dsp_tests.cpp)
target_include_directories(PostGrapeTests PRIVATE src)

# Manual network test of the update checker (not built by default).
juce_add_console_app(PostGrapeUpdateTest PRODUCT_NAME "PostGrapeUpdateTest")
set_target_properties(PostGrapeUpdateTest PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(PostGrapeUpdateTest PRIVATE tests/update_test.cpp src/UpdateChecker.cpp)
target_compile_definitions(PostGrapeUpdateTest PRIVATE JucePlugin_VersionString="1.0.0" JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(PostGrapeUpdateTest PRIVATE juce::juce_events PUBLIC juce::juce_recommended_config_flags)
