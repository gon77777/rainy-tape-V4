cmake_minimum_required(VERSION 3.22)
project(TapeLofi VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Formats: VST3 (Win/macOS/Linux) + AU (macOS only, ignored elsewhere)
# + Standalone for quick local checks.
juce_add_plugin(TapeLofi
    COMPANY_NAME            "AMA51"
    PLUGIN_MANUFACTURER_CODE Am51
    PLUGIN_CODE             Tl01
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Rainy Tape"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Distortion Modulation
    AU_MAIN_TYPE             kAudioUnitType_Effect)

juce_generate_juce_header(TapeLofi)

# Bundled fonts (Zen Antique subset: SIL OFL 1.1 / Yellowtail: Apache 2.0).
# Licence texts live next to the font files in assets/fonts/.
juce_add_binary_data(RainyAssets SOURCES
    assets/fonts/ZenAntiqueSubset.ttf
    assets/fonts/Yellowtail.ttf)
set_target_properties(RainyAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(TapeLofi PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/RainyLookAndFeel.cpp
    src/ui/Widgets.cpp
    src/ui/PanelTexture.cpp
    src/ui/PresetTabs.cpp
    src/dsp/TapeEngine.cpp)

target_include_directories(TapeLofi PRIVATE src)

# Source files are UTF-8 (Japanese UI strings and preset names).
if(MSVC)
    target_compile_options(TapeLofi PRIVATE /utf-8)
endif()

target_compile_definitions(TapeLofi PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TapeLofi PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_dsp
    RainyAssets
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
