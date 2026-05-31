cmake_minimum_required(VERSION 3.16)
project(HavenTools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
        imguifiledialog
        GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
        GIT_TAG v0.6.7
)
FetchContent_MakeAvailable(imguifiledialog)

FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG v2.9.3
)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)
target_include_directories(zlibstatic PUBLIC ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})

add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw d3d11 dxgi d3dcompiler)

add_library(imguifiledialog_lib STATIC
        ${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp
)
target_include_directories(imguifiledialog_lib PUBLIC ${imguifiledialog_SOURCE_DIR})
target_link_libraries(imguifiledialog_lib PUBLIC imgui_lib)

file(READ "${CMAKE_SOURCE_DIR}/src/model_names.csv" CSV_CONTENT HEX)
string(LENGTH "${CSV_CONTENT}" CSV_HEX_LENGTH)
math(EXPR CSV_LENGTH "${CSV_HEX_LENGTH} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," CSV_ARRAY "${CSV_CONTENT}")
string(REGEX REPLACE ",$" "" CSV_ARRAY "${CSV_ARRAY}")
file(WRITE "${CMAKE_BINARY_DIR}/model_names_csv.h"
        "#pragma once
unsigned char model_names_csv[] = {
${CSV_ARRAY}
};
unsigned int model_names_csv_len = ${CSV_LENGTH};
")

function(embed_directory DIR_NAME)
    file(GLOB_RECURSE FILES RELATIVE "${CMAKE_SOURCE_DIR}/src/dependencies/${DIR_NAME}"
            "${CMAKE_SOURCE_DIR}/src/dependencies/${DIR_NAME}/*")

    set(HEADER "")
    foreach(F ${FILES})
        file(READ "${CMAKE_SOURCE_DIR}/src/dependencies/${DIR_NAME}/${F}" DATA HEX)
        string(LENGTH "${DATA}" LEN)
        math(EXPR LEN "${LEN} / 2")
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," DATA_ARRAY "${DATA}")
        string(REGEX REPLACE ",$" "" DATA_ARRAY "${DATA_ARRAY}")
        string(REPLACE "/" "_" VAR "${DIR_NAME}_${F}")
        string(REPLACE "." "_" VAR "${VAR}")
        string(APPEND HEADER
                "unsigned char ${VAR}[] = {${DATA_ARRAY}};
unsigned int ${VAR}_len = ${LEN};
")
    endforeach()

    file(WRITE "${CMAKE_BINARY_DIR}/${DIR_NAME}_embedded.h"
            "#pragma once
${HEADER}
            ")
endfunction()

embed_directory(msh)
embed_directory(mmh)
embed_directory(ani)
embed_directory(excel)
embed_directory(hashes)

# FontAwesome (now under dependencies/Assets/otfs)
file(GLOB FA_SOLID_FILES "${CMAKE_SOURCE_DIR}/src/dependencies/Assets/otfs/*Solid*"
                        "${CMAKE_SOURCE_DIR}/src/dependencies/Assets/otfs/*solid*")
list(LENGTH FA_SOLID_FILES FA_COUNT)
if(FA_COUNT GREATER 0)
    list(GET FA_SOLID_FILES 0 FA_SOLID_FILE)
    file(READ "${FA_SOLID_FILE}" FA_DATA HEX)
    string(LENGTH "${FA_DATA}" FA_HEX_LEN)
    math(EXPR FA_LEN "${FA_HEX_LEN} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FA_ARRAY "${FA_DATA}")
    string(REGEX REPLACE ",$" "" FA_ARRAY "${FA_ARRAY}")
    file(WRITE "${CMAKE_BINARY_DIR}/fa_solid_embedded.h"
            "#pragma once
static unsigned char fa_solid_otf[] = {
${FA_ARRAY}
};
static unsigned int fa_solid_otf_len = ${FA_LEN};
")
    message(STATUS "Embedded FontAwesome Solid: ${FA_SOLID_FILE} (${FA_LEN} bytes)")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/fa_solid_embedded.h"
            "#pragma once
static unsigned char fa_solid_otf[] = {0};
static unsigned int fa_solid_otf_len = 0;
")
    message(WARNING "FontAwesome Solid font not found in src/dependencies/Assets/otfs/")
endif()

# Roboto-Regular (global UI font)
set(ROBOTO_FILE "${CMAKE_SOURCE_DIR}/src/dependencies/Fonts/Roboto-Regular.ttf")
if(EXISTS "${ROBOTO_FILE}")
    file(READ "${ROBOTO_FILE}" ROBOTO_DATA HEX)
    string(LENGTH "${ROBOTO_DATA}" ROBOTO_HEX_LEN)
    math(EXPR ROBOTO_LEN "${ROBOTO_HEX_LEN} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ROBOTO_ARRAY "${ROBOTO_DATA}")
    string(REGEX REPLACE ",$" "" ROBOTO_ARRAY "${ROBOTO_ARRAY}")
    file(WRITE "${CMAKE_BINARY_DIR}/roboto_embedded.h"
            "#pragma once
static unsigned char roboto_ttf[] = {
${ROBOTO_ARRAY}
};
static unsigned int roboto_ttf_len = ${ROBOTO_LEN};
")
    message(STATUS "Embedded Roboto-Regular: ${ROBOTO_FILE} (${ROBOTO_LEN} bytes)")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/roboto_embedded.h"
            "#pragma once
static unsigned char roboto_ttf[] = {0};
static unsigned int roboto_ttf_len = 0;
")
    message(WARNING "Roboto-Regular.ttf not found at ${ROBOTO_FILE}")
endif()

# SpeedTree RT 4.1 runtime, compiled directly into the app (no DLL/exe).
# Vendored from the open-source SpeedTree SDK (Source-Authors/speedtreesdk, branch
# speedtree_4_1). FULL_SPEEDTREE_LIB strips the (decorative) license check;
# UPVECTOR_POS_Z matches SpeedTree's native / Eclipse engine Z-up convention.
set(SPEEDTREE_DIR "${CMAKE_SOURCE_DIR}/src/SpeedTree")
file(GLOB_RECURSE SPEEDTREE_SOURCES "${SPEEDTREE_DIR}/*.cpp")
add_library(speedtree STATIC ${SPEEDTREE_SOURCES})
target_include_directories(speedtree PUBLIC "${SPEEDTREE_DIR}")
target_include_directories(speedtree PRIVATE
        "${SPEEDTREE_DIR}/EvalKey_Source"
        "${SPEEDTREE_DIR}/LibFilename_Source"
        "${SPEEDTREE_DIR}/LibGlobals_Source"
        "${SPEEDTREE_DIR}/LibRandom_Source"
        "${SPEEDTREE_DIR}/LibSpline_Source"
        "${SPEEDTREE_DIR}/LibVector_Source"
)
target_compile_definitions(speedtree PUBLIC FULL_SPEEDTREE_LIB UPVECTOR_POS_Z)
if(MSVC)
    target_compile_definitions(speedtree PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(speedtree PRIVATE /w)  # vendored 3rd-party: silence warnings
else()
    target_compile_options(speedtree PRIVATE -w)
endif()

set(BLENDER_ADDON_PATH "${CMAKE_SOURCE_DIR}/src/python/havenarea_importer.zip")
if(EXISTS "${BLENDER_ADDON_PATH}")
    file(READ "${BLENDER_ADDON_PATH}" ADDON_DATA HEX)
    string(LENGTH "${ADDON_DATA}" ADDON_HEX_LEN)
    math(EXPR ADDON_LEN "${ADDON_HEX_LEN} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ADDON_ARRAY "${ADDON_DATA}")
    string(REGEX REPLACE ",$" "" ADDON_ARRAY "${ADDON_ARRAY}")
    file(WRITE "${CMAKE_BINARY_DIR}/blender_addon_embedded.h"
            "#pragma once
static const unsigned char kBlenderAddonZip[] = {
${ADDON_ARRAY}
};
static const unsigned int kBlenderAddonZipSize = ${ADDON_LEN};
")
    message(STATUS "Embedded Blender addon: ${BLENDER_ADDON_PATH} (${ADDON_LEN} bytes)")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/blender_addon_embedded.h"
            "#pragma once
static const unsigned char kBlenderAddonZip[] = {0};
static const unsigned int kBlenderAddonZipSize = 0;
")
    message(WARNING "Blender addon not found at ${BLENDER_ADDON_PATH}")
endif()

add_executable(HavenTools
        src/main.cpp
        src/version.h

        # core
        src/core/types.cpp
        src/core/types.h
        src/core/Mesh.h
        src/core/fnv.cpp
        src/core/fnv.h
        src/core/Blowfish.cpp
        src/core/Blowfish.h

        # formats
        src/formats/erf.cpp
        src/formats/erf.h
        src/formats/Gff.cpp
        src/formats/Gff.h
        src/formats/gff32.cpp
        src/formats/gff32.h
        src/formats/Gff4FieldNames.h
        src/formats/gda.cpp
        src/formats/gda.h

        # loaders
        src/loaders/model_loader.cpp
        src/loaders/model_loader.h
        src/loaders/mmh_loader.cpp
        src/loaders/mmh_loader.h
        src/loaders/phy_loader.cpp
        src/loaders/dds_loader.cpp
        src/loaders/dds_loader.h
        src/loaders/tnt_loader.cpp
        src/loaders/tnt_loader.h
        src/loaders/level_loader.cpp
        src/loaders/level_loader.h
        src/loaders/terrain_loader.cpp
        src/loaders/terrain_loader.h
        src/loaders/Rml_Loader.cpp
        src/loaders/Rml_Loader.h
        src/loaders/spt.cpp
        src/loaders/spt.h
        src/loaders/LevelDatabase.h

        # render
        src/render/renderer.cpp
        src/render/renderer.h
        src/render/animation.cpp
        src/render/animation.h

        # shaders / d3d
        src/Shaders/shader.cpp
        src/Shaders/shader.h
        src/Shaders/D3d_Context.cpp
        src/Shaders/D3d_Context.h

        # X360 / PS3 platform support (big-endian Aurora variants)
        src/X360/X360_Platform.h
        src/X360/X360_GffQuirks.h
        src/X360/X360_Texture.h
        src/X360/X360_Texture.cpp
        src/X360/X360_Animation.cpp
        src/X360/X360_Iso.h
        src/X360/X360_Iso.cpp

        # ui
        src/ui/ui.h
        src/ui/ui_internal.h
        src/ui/ui_main.cpp
        src/ui/ui_audio.cpp
        src/ui/ui_browser.cpp
        src/ui/ui_browser_mesh.cpp
        # CharacterDesigner module (replaces ui_character.cpp + mor_loader)
        src/CharacterDesigner/CharacterDesigner.h
        src/CharacterDesigner/CharacterDesigner_Internal.h
        src/CharacterDesigner/CharacterDesigner.cpp
        src/CharacterDesigner/MorphLoader.h
        src/CharacterDesigner/MorphLoader.cpp
        src/CharacterDesigner/UI/CharacterDesigner_UI.cpp
        src/CharacterDesigner/UI/HeadTab.cpp
        src/CharacterDesigner/UI/FaceTab.cpp
        src/CharacterDesigner/UI/ArmorTab.cpp
        src/CharacterDesigner/UI/ClothesTab.cpp
        src/CharacterDesigner/UI/BootsTab.cpp
        src/CharacterDesigner/UI/GlovesTab.cpp
        src/CharacterDesigner/UI/HelmetTab.cpp
        src/CharacterDesigner/UI/WeaponsTab.cpp
        src/ui/ui_gda.cpp
        src/ui/ui_helpers.cpp
        src/ui/ui_settings.cpp
        src/ui/ui_speedtree.cpp
        src/ui/ui_windows.cpp
        src/ui/gffviewer.cpp
        src/ui/gffviewer.h

        # io (import/export)
        src/io/import.cpp
        src/io/import.h
        src/io/export.cpp
        src/io/export.h
        src/io/terrain_export.cpp
        src/io/terrain_export.h

        # update / installer
        src/update/app.rc
        src/update/update.cpp
        src/update/update.h
        src/update/about_text.h
        src/update/changelog_text.h

        # dependencies (header-only stuff referenced directly)
        src/dependencies/Fonts/IconsFontAwesome6.h
)

target_include_directories(HavenTools PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/formats
        ${CMAKE_SOURCE_DIR}/src/loaders
        ${CMAKE_SOURCE_DIR}/src/render
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/io
        ${CMAKE_SOURCE_DIR}/src/X360
        ${CMAKE_SOURCE_DIR}/src/CharacterDesigner
        ${CMAKE_SOURCE_DIR}/src/dependencies
        ${CMAKE_BINARY_DIR}
)

target_link_libraries(HavenTools PRIVATE imgui_lib imguifiledialog_lib tinygltf zlibstatic speedtree)

if(WIN32)
    target_compile_definitions(HavenTools PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:_CRT_SECURE_NO_WARNINGS>
            $<$<COMPILE_LANGUAGE:C,CXX>:NOMINMAX>
            $<$<COMPILE_LANGUAGE:C,CXX>:WIN32_LEAN_AND_MEAN>
    )
    target_link_libraries(HavenTools PRIVATE
            winmm winhttp version
            mfplat mfreadwrite mfuuid ole32
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(HavenTools PRIVATE dl pthread)
endif()
