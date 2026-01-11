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

add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

if(WIN32)
    target_link_libraries(imgui_lib PUBLIC opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
endif()

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

add_executable(HavenTools
        src/main.cpp
        src/types.cpp
        src/types.h
        src/erf.cpp
        src/erf.h
        src/fnv.cpp
        src/fnv.h
        src/Gff.cpp
        src/Gff.h
        src/Mesh.h
        src/model_loader.cpp
        src/model_loader.h
        src/dds_loader.cpp
        src/dds_loader.h
        src/animation.cpp
        src/animation.h
        src/mmh_loader.cpp
        src/mmh_loader.h
        src/phy_loader.cpp
        src/renderer.cpp
        src/renderer.h
        src/ui.h
        src/ui_internal.h
        src/ui_settings.cpp
        src/ui_audio.cpp
        src/ui_helpers.cpp
        src/ui_browser.cpp
        src/ui_windows.cpp
        src/ui_character.cpp
        src/ui_main.cpp
        src/export.cpp
        src/export.h
        src/mor_loader.cpp
        src/mor_loader.h
        src/tnt_loader.cpp
        src/tnt_loader.h
        src/shader.cpp
        src/shader.h
        src/version.h
        src/update/app.rc
        src/update/update.cpp
        src/update/update.h
        src/update/about_text.h
        src/update/changelog_text.h
        src/import.cpp
        src/import.h
        src/gda.cpp
        src/gda.h
        src/ui_gda.cpp
        src/level_loader.cpp
        src/level_loader.h
        src/terrain_loader.cpp
        src/terrain_loader.h
        src/gff32.cpp
        src/gff32.h
        src/gffviewer.cpp
        src/gffviewer.h
)

target_include_directories(HavenTools PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
)

target_link_libraries(HavenTools PRIVATE imgui_lib imguifiledialog_lib tinygltf)

if(WIN32)
    target_compile_definitions(HavenTools PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(HavenTools PRIVATE winmm winhttp version)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(HavenTools PRIVATE dl pthread)
endif()
