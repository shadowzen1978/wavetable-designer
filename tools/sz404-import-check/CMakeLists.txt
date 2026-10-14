cmake_minimum_required(VERSION 3.25)
project(wtd_sz404_import_check LANGUAGES CXX)
if(NOT MSVC OR NOT WIN32)
    message(FATAL_ERROR "This optional harness requires an existing Windows x64 Release SZ404 build")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SZ404_REPO "" CACHE PATH "Sister ts404-remake checkout")
set(SZ404_BUILD "" CACHE PATH "Existing Release build of that checkout")
foreach(file IN ITEMS "${SZ404_BUILD}/SZ404_artefacts/Release/SZ404_SharedCode.lib"
    "${SZ404_BUILD}/sz404_dsp.lib" "${SZ404_BUILD}/SZ404BinaryData.lib")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Build SZ404 separately first; missing ${file}")
    endif()
endforeach()
add_executable(wtd_sz404_import_check main.cpp)
target_include_directories(wtd_sz404_import_check PRIVATE
    "${SZ404_REPO}/plugin/Source" "${SZ404_BUILD}/_deps/juce-src/modules")
target_compile_definitions(wtd_sz404_import_check PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(wtd_sz404_import_check PRIVATE
    "${SZ404_BUILD}/SZ404_artefacts/Release/SZ404_SharedCode.lib"
    "${SZ404_BUILD}/sz404_dsp.lib" "${SZ404_BUILD}/SZ404BinaryData.lib")
target_link_options(wtd_sz404_import_check PRIVATE /LTCG)
