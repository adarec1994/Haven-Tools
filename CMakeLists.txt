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

# Embed model_names.csv into the executable
file(READ "${CMAKE_SOURCE_DIR}/src/model_names.csv" CSV_CONTENT HEX)
string(LENGTH "${CSV_CONTENT}" CSV_HEX_LENGTH)
math(EXPR CSV_LENGTH "${CSV_HEX_LENGTH} / 2")

# Convert hex to C array
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," CSV_ARRAY "${CSV_CONTENT}")
string(REGEX REPLACE ",$" "" CSV_ARRAY "${CSV_ARRAY}")

file(WRITE "${CMAKE_BINARY_DIR}/model_names_csv.h"
        "#pragma once
unsigned char model_names_csv[] = {
${CSV_ARRAY}
};
unsigned int model_names_csv_len = ${CSV_LENGTH};
")

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
)

target_include_directories(HavenTools PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
target_link_libraries(HavenTools PRIVATE imgui_lib imguifiledialog_lib)

if(WIN32)
    target_compile_definitions(HavenTools PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(HavenTools PRIVATE winmm)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(HavenTools PRIVATE dl pthread)
endif()