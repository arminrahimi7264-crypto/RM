cmake_minimum_required(VERSION 3.22)
project(SimpleSynth VERSION 1.0.0)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(SimpleSynth
  COMPANY_NAME "YourName"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  PLUGIN_MANUFACTURER_CODE Ysyn
  PLUGIN_CODE Ssyn
  FORMATS VST3 Standalone
  PRODUCT_NAME "Simple Synth")
target_sources(SimpleSynth PRIVATE Source/PluginProcessor.cpp)
target_compile_definitions(SimpleSynth PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SimpleSynth PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
