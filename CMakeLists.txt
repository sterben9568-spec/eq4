cmake_minimum_required(VERSION 3.22)
project(RBT_EQ VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RBT_EQ
  COMPANY_NAME "RBT Audio"
  PRODUCT_NAME "RBT EQ"
  PLUGIN_MANUFACTURER_CODE Rbta
  PLUGIN_CODE RbEQ
  FORMATS VST3 Standalone
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES "Fx" "EQ"
  AU_MAIN_TYPE kAudioUnitType_Effect
)

juce_generate_juce_header(RBT_EQ)

target_sources(RBT_EQ PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
)

target_compile_definitions(RBT_EQ PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(RBT_EQ PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags
  PUBLIC juce::juce_recommended_lto_flags
  PUBLIC juce::juce_recommended_warning_flags
)
