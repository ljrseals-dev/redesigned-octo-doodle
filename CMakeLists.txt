cmake_minimum_required(VERSION 3.20)
project(CosmicSynthEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COSMIC_BUILD_TESTS "Build unit tests" ON)

add_library(cosmic_dsp INTERFACE)
target_include_directories(cosmic_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if(COSMIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# Optional JUCE plugin – developer must supply JUCE_PATH
if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "JUCE found at ${JUCE_PATH} — enabling plugin target")
    add_subdirectory(${JUCE_PATH} JUCE)

    juce_add_plugin(CosmicSynth
        COMPANY_NAME "Cosmic DSP"
        PLUGIN_MANUFACTURER_CODE Csmc
        PLUGIN_CODE Csyn
        FORMATS VST3 Standalone
        PRODUCT_NAME "Cosmic Synthesizer Engine"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    )

    target_sources(CosmicSynth PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
    )

    target_compile_definitions(CosmicSynth
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JucePlugin_Name="Cosmic Synthesizer Engine"
    )

    target_link_libraries(CosmicSynth
        PRIVATE
            cosmic_dsp
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
else()
    message(STATUS "JUCE_PATH not set — building DSP + tests only")
endif()

message(STATUS "Cosmic Synthesizer Engine ${PROJECT_VERSION} configured")
