# SPDX-License-Identifier: GPL-3.0-only
# ATLATL FM-1, the FM-1 plugin (skiff-atlatl): CLAP, and through clap-wrapper VST3, AUv2 (macOS) and a standalone app.
#   cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release && cmake --build build/plugin --config Release
# The dependencies come from GitHub (FetchContent; the VST3 and AU SDKs through clap-wrapper), or from local copies:
# -DFM1_DEPS=/path with clap, clap-wrapper, choc, rtmidi in it.
cmake_minimum_required(VERSION 3.21)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
if(APPLE)
    project(fm1-plugin VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(fm1-plugin VERSION 0.1.0 LANGUAGES C CXX)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
if(MSVC)
    add_compile_options(/utf-8 /bigobj)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(FM1_DEPS "" CACHE PATH "local copies of the dependencies (else FetchContent)")
include(FetchContent)
function(fm1_dep name repo tag)
    if(FM1_DEPS AND EXISTS "${FM1_DEPS}/${name}")
        set(${name}_SOURCE_DIR "${FM1_DEPS}/${name}" PARENT_SCOPE)
    else()
        FetchContent_Declare(${name} GIT_REPOSITORY https://github.com/${repo} GIT_TAG ${tag} GIT_SHALLOW TRUE)
        FetchContent_GetProperties(${name})
        if(NOT ${name}_POPULATED)
            FetchContent_Populate(${name})
        endif()
        set(${name}_SOURCE_DIR "${${name}_SOURCE_DIR}" PARENT_SCOPE)
    endif()
endfunction()
fm1_dep(clap free-audio/clap 1.2.10)
fm1_dep(clap-wrapper free-audio/clap-wrapper v0.17.0)
fm1_dep(choc Tracktion/choc 8af031c2ba4f5723f4b0a5ca14e0b4984af15b0e)
fm1_dep(rtmidi thestk/rtmidi 6.1.0)
fm1_dep(miniaudio mackron/miniaudio 0.11.26)

add_subdirectory(${clap_SOURCE_DIR} clap EXCLUDE_FROM_ALL)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "")
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE CACHE BOOL "")
add_subdirectory(${clap-wrapper_SOURCE_DIR} clap-wrapper EXCLUDE_FROM_ALL)

# the editor's files, built in
set(WEB ${CMAKE_CURRENT_SOURCE_DIR}/../web)
set(FM1_CONV_WASM ${CMAKE_CURRENT_SOURCE_DIR}/../build/conv/conv.wasm CACHE FILEPATH "the converter (Emscripten's)")
set(EMBED
    "editor.html=${WEB}/editor.html" "fukiai.ttf=${WEB}/fukiai.ttf" "fm1backup.js=${WEB}/fm1backup.js"
    "conv/conv.js=${WEB}/conv/conv.js" "conv/conv.wasm=${FM1_CONV_WASM}"
    "fonts/BarlowCondensed-Medium.ttf=${WEB}/fonts/BarlowCondensed-Medium.ttf"
    "fonts/BarlowCondensed-Bold.ttf=${WEB}/fonts/BarlowCondensed-Bold.ttf"
    "fonts/InterTight-Latin.woff2=${WEB}/fonts/InterTight-Latin.woff2"
    "shim.js=${CMAKE_CURRENT_SOURCE_DIR}/src/shim.js")
set(EMBED_DEPS "")
foreach(pair ${EMBED})
    string(REGEX REPLACE "^[^=]*=" "" path "${pair}")
    if(EXISTS "${path}")
        list(APPEND EMBED_DEPS "${path}")
    endif()
endforeach()
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.cpp
    COMMAND ${CMAKE_COMMAND} "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/resources.cpp" "-DFILES=${EMBED}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS ${EMBED_DEPS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake VERBATIM)

# the device link and CAPTURE, testable on their own
add_library(fm1-link STATIC src/link.cpp src/port_rtmidi.cpp src/capture.cpp ${rtmidi_SOURCE_DIR}/RtMidi.cpp
            ${miniaudio_SOURCE_DIR}/miniaudio.c)
target_include_directories(fm1-link PUBLIC src ${rtmidi_SOURCE_DIR} ${miniaudio_SOURCE_DIR})
target_compile_definitions(fm1-link PUBLIC MA_NO_ENCODING MA_NO_DECODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER
                           MA_NO_NODE_GRAPH MA_NO_ENGINE)
find_package(Threads REQUIRED)
target_link_libraries(fm1-link PUBLIC Threads::Threads)
if(APPLE)
    target_compile_definitions(fm1-link PUBLIC __MACOSX_CORE__)
    target_link_libraries(fm1-link PUBLIC "-framework CoreMIDI" "-framework CoreAudio" "-framework CoreFoundation"
                          "-framework AudioToolbox" "-framework AudioUnit")
elseif(WIN32)
    target_compile_definitions(fm1-link PUBLIC __WINDOWS_MM__ NOMINMAX)
    target_link_libraries(fm1-link PUBLIC winmm)
else()
    find_package(ALSA REQUIRED)
    target_compile_definitions(fm1-link PUBLIC __LINUX_ALSA__)
    target_link_libraries(fm1-link PUBLIC ALSA::ALSA ${CMAKE_DL_LIBS} m)
endif()

# the plugin, as a static library the formats are made from
add_library(fm1-impl STATIC src/plugin.cpp src/gui.cpp ${CMAKE_CURRENT_BINARY_DIR}/resources.cpp)
target_include_directories(fm1-impl PRIVATE src ${choc_SOURCE_DIR})
target_compile_definitions(fm1-impl PRIVATE FM1_PLUGIN_VERSION="${PROJECT_VERSION}")
target_link_libraries(fm1-impl PUBLIC clap clap-wrapper-extensions fm1-link)
if(APPLE)
    target_sources(fm1-impl PRIVATE src/gui_mac.mm)
    set_source_files_properties(src/gui_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(fm1-impl PUBLIC "-framework WebKit" "-framework Cocoa")
elseif(WIN32)
    target_link_libraries(fm1-impl PUBLIC ole32 oleaut32 user32 shlwapi version)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0 webkit2gtk-4.1)
    target_link_libraries(fm1-impl PUBLIC PkgConfig::GTK)
endif()

set(FORMATS CLAP VST3)
if(APPLE)
    list(APPEND FORMATS AUV2)
endif()
make_clapfirst_plugins(
    TARGET_NAME fm1
    IMPL_TARGET fm1-impl
    OUTPUT_NAME "ATLATL FM-1"
    ENTRY_SOURCE src/entry.cpp
    BUNDLE_IDENTIFIER "io.github.likst4.skiff-atlatl.fm1"
    BUNDLE_VERSION ${PROJECT_VERSION}
    COPY_AFTER_BUILD FALSE
    WINDOWS_FOLDER_VST3 TRUE
    PLUGIN_FORMATS ${FORMATS}
    ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out
    AUV2_MANUFACTURER_NAME "ATLATL"
    AUV2_MANUFACTURER_CODE "Atlt"
    AUV2_SUBTYPE_CODE "Fm1s"
    AUV2_INSTRUMENT_TYPE "aumu"
    STANDALONE_CONFIGURATIONS standalone "ATLATL FM-1" "io.github.likst4.skiff-atlatl.fm1"
    STANDALONE_MACOS_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/atlatl.icns    # (tools/make_logo.py)
    STANDALONE_WINDOWS_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/atlatl.ico
)

option(FM1_TESTS "the link's tests" ON)
if(FM1_TESTS)
    add_executable(fm1-link-test tests/link_test.cpp)
    target_link_libraries(fm1-link-test PRIVATE fm1-link)
    add_executable(fm1-capture-test tests/capture_test.cpp)
    target_link_libraries(fm1-capture-test PRIVATE fm1-link)
    enable_testing()
    add_test(NAME link COMMAND fm1-link-test)
    add_test(NAME capture COMMAND fm1-capture-test)
endif()
