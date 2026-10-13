cmake_minimum_required(VERSION 3.25)
project(wavetable_designer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WTD_BUILD_APP "Build the JUCE desktop application" ON)
include(CTest)
include(FetchContent)
# The pinned doctest subproject predates CMake 4's minimum policy floor.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

add_library(wavetable_core STATIC src/core/Waveform.cpp src/core/EditorModel.cpp)
target_include_directories(wavetable_core PUBLIC src)
target_compile_options(wavetable_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>)

if(BUILD_TESTING)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        # doctest 2.4.11, pinned to its exact revision.
        GIT_TAG ae7a13539fb71f270b87eb2e874fbac80bc8dda2)
    FetchContent_MakeAvailable(doctest)
    add_executable(wavetable_tests tests/WaveformTests.cpp tests/EditorModelTests.cpp tests/SmoothingTests.cpp tests/RegionalSmoothingTests.cpp tests/HorizontalViewportTests.cpp tests/EdgeSmoothingTests.cpp tests/JitterTests.cpp)
    target_link_libraries(wavetable_tests PRIVATE wavetable_core doctest::doctest)
    add_test(NAME waveform_contract COMMAND wavetable_tests)
endif()

if(WTD_BUILD_APP)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        # JUCE 8.0.4, pinned to its exact revision.
        GIT_TAG 51d11a2be6d5c97ccf12b4e5e827006e19f0555a)
    FetchContent_MakeAvailable(JUCE)
    juce_add_gui_app(WavetableDesigner
        PRODUCT_NAME "Wavetable Designer"
        COMPANY_NAME "Shadowzen Labs"
        BUNDLE_ID com.shadowzenlabs.wavetabledesigner)
    target_sources(WavetableDesigner PRIVATE
        src/app/Main.cpp
        src/app/MainComponent.cpp
        src/app/DrawingSmokeCheck.cpp
        src/app/SmoothingSmokeCheck.cpp
        src/app/RegionalSmoothingSmokeCheck.cpp
        src/app/EdgeSmoothingSmokeCheck.cpp
        src/app/JitterSmokeCheck.cpp
        src/app/WaveformView.cpp)
    target_compile_definitions(WavetableDesigner PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:WavetableDesigner,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:WavetableDesigner,JUCE_VERSION>")
    target_link_libraries(WavetableDesigner PRIVATE
        wavetable_core
        juce::juce_gui_basics
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
