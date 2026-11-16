cmake_minimum_required(VERSION 3.22)
project(OctaveForge VERSION 1.0.0 LANGUAGES C CXX)

if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(OctaveForge
    COMPANY_NAME "CEEM"
    PRODUCT_NAME "Octave Forge"
    PLUGIN_MANUFACTURER_CODE Ceem
    PLUGIN_CODE OcFg
    FORMATS VST3
    VST3_CATEGORIES Fx Pitch Shift)
target_sources(OctaveForge PRIVATE Plugin.cpp)
target_compile_features(OctaveForge PRIVATE cxx_std_17)
target_compile_definitions(OctaveForge PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(OctaveForge PRIVATE juce::juce_audio_processors)


