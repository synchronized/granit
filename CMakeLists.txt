# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)

project(
  granit
  VERSION 0.1.0
  DESCRIPTION "Vulkan-based C++20 rendering library"
  LANGUAGES C CXX
)

option(BUILD_SHARED_LIBS "构建共享库" ON)
option(GRANIT_BUILD_TESTING "构建 Granit 测试" ON)
option(GRANIT_BUILD_EXAMPLES "构建 Granit 示例" ${PROJECT_IS_TOP_LEVEL})
option(GRANIT_BUILD_BENCHMARKS "构建 Granit benchmark 程序" OFF)
option(GRANIT_BUILD_TOOLS "构建 Granit 离线工具" OFF)
option(GRANIT_BUILD_INTEGRATION_SDL3 "构建 SDL3 可选集成组件" OFF)
option(GRANIT_BUILD_INTEGRATION_IMGUI "构建 ImGui 可选集成组件" OFF)
option(GRANIT_FETCH_INTEGRATION_DEPENDENCIES "下载锁定版本的 SDL3 与 ImGui 集成依赖" OFF)
option(GRANIT_ENABLE_XCB "在 Linux 上启用 XCB Surface 与 Window 后端" ON)
option(GRANIT_ENABLE_WAYLAND "在 Linux 上启用 Wayland Surface 与 Window 后端" ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(utility)

set(GRANIT_HAS_XCB OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GRANIT_ENABLE_XCB)
  find_path(GRANIT_XCB_INCLUDE_DIR xcb/xcb.h)
  find_library(GRANIT_XCB_LIBRARY NAMES xcb)
  if(GRANIT_XCB_INCLUDE_DIR AND GRANIT_XCB_LIBRARY)
    set(GRANIT_HAS_XCB ON)
    message(STATUS "Granit XCB Surface and Window backends enabled")
  else()
    message(STATUS "Granit XCB backends disabled: XCB development package not found")
  endif()
endif()

set(GRANIT_HAS_WAYLAND OFF)
set(GRANIT_HAS_WAYLAND_SHELL OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GRANIT_ENABLE_WAYLAND)
  find_path(GRANIT_WAYLAND_INCLUDE_DIR wayland-client.h)
  find_library(GRANIT_WAYLAND_LIBRARY NAMES wayland-client)
  if(GRANIT_WAYLAND_INCLUDE_DIR AND GRANIT_WAYLAND_LIBRARY)
    set(GRANIT_HAS_WAYLAND ON)
    message(STATUS "Granit Wayland Surface backend enabled")
    find_program(GRANIT_WAYLAND_SCANNER wayland-scanner)
    find_file(
      GRANIT_XDG_SHELL_PROTOCOL
      xdg-shell.xml
      PATHS /usr/share /usr/local/share
      PATH_SUFFIXES wayland-protocols/stable/xdg-shell
    )
    if(GRANIT_WAYLAND_SCANNER AND GRANIT_XDG_SHELL_PROTOCOL)
      set(GRANIT_HAS_WAYLAND_SHELL ON)
      message(STATUS "Granit Wayland xdg-shell Window backend enabled")
    else()
      message(STATUS "Granit Wayland Window backend disabled: protocol tools not found")
    endif()
  else()
    message(STATUS "Granit Wayland Surface backend disabled: Wayland development package not found")
  endif()
endif()

set(
  GRANIT_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/granit"
  CACHE STRING
  "Granit CMake 包配置安装目录"
)

if(GRANIT_BUILD_INTEGRATION_SDL3)
  if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 3.2 CONFIG QUIET)
  endif()
  if(NOT TARGET SDL3::SDL3 AND GRANIT_FETCH_INTEGRATION_DEPENDENCIES)
    include(FetchContent)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      granit_sdl3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.10
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(granit_sdl3)
    set(GRANIT_SDL3_FETCHED TRUE)
  endif()
  if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "已启用 SDL3 Integration，但未找到 SDL3::SDL3（最低版本 3.2）")
  endif()
  set(GRANIT_SDL3_TARGET SDL3::SDL3)
endif()

if(GRANIT_BUILD_INTEGRATION_IMGUI)
  if(NOT GRANIT_IMGUI_FETCHED AND NOT TARGET imgui::imgui AND NOT TARGET ImGui::ImGui AND
     NOT TARGET imgui)
    find_package(imgui CONFIG QUIET)
  endif()
  if(NOT TARGET imgui::imgui AND NOT TARGET ImGui::ImGui AND NOT TARGET imgui AND
     GRANIT_FETCH_INTEGRATION_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Declare(
      granit_imgui
      GIT_REPOSITORY https://github.com/ocornut/imgui.git
      GIT_TAG v1.92.9
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(granit_imgui)
    add_library(
      granit_imgui_dependency STATIC
      "${granit_imgui_SOURCE_DIR}/imgui.cpp"
      "${granit_imgui_SOURCE_DIR}/imgui_draw.cpp"
      "${granit_imgui_SOURCE_DIR}/imgui_tables.cpp"
      "${granit_imgui_SOURCE_DIR}/imgui_widgets.cpp"
    )
    target_include_directories(granit_imgui_dependency PUBLIC "${granit_imgui_SOURCE_DIR}")
    target_compile_features(granit_imgui_dependency PUBLIC cxx_std_20)
    set_target_properties(
      granit_imgui_dependency
      PROPERTIES POSITION_INDEPENDENT_CODE YES FOLDER "Third Party"
    )
    set(GRANIT_IMGUI_FETCHED TRUE)
  endif()
  if(NOT GRANIT_IMGUI_FETCHED AND NOT TARGET imgui::imgui AND NOT TARGET ImGui::ImGui AND
     NOT TARGET imgui)
    message(FATAL_ERROR "已启用 ImGui Integration，但未找到 imgui::imgui、ImGui::ImGui 或 imgui")
  endif()
  if(GRANIT_IMGUI_FETCHED)
    set(GRANIT_IMGUI_TARGET granit_imgui_dependency)
  elseif(TARGET imgui::imgui)
    set(GRANIT_IMGUI_TARGET imgui::imgui)
  elseif(TARGET ImGui::ImGui)
    set(GRANIT_IMGUI_TARGET ImGui::ImGui)
  else()
    set(GRANIT_IMGUI_TARGET imgui)
  endif()
endif()

include(CTest)
add_subdirectory(3rd)

if(GRANIT_HAS_WAYLAND_SHELL)
  set(granit_wayland_protocol_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
  set(granit_xdg_shell_header "${granit_wayland_protocol_dir}/xdg-shell-client-protocol.h")
  set(granit_xdg_shell_source "${granit_wayland_protocol_dir}/xdg-shell-protocol.c")
  add_custom_command(
    OUTPUT "${granit_xdg_shell_header}" "${granit_xdg_shell_source}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_wayland_protocol_dir}"
    COMMAND
      "${GRANIT_WAYLAND_SCANNER}" client-header "${GRANIT_XDG_SHELL_PROTOCOL}"
      "${granit_xdg_shell_header}"
    COMMAND
      "${GRANIT_WAYLAND_SCANNER}" private-code "${GRANIT_XDG_SHELL_PROTOCOL}"
      "${granit_xdg_shell_source}"
    DEPENDS "${GRANIT_XDG_SHELL_PROTOCOL}"
    VERBATIM
  )
  add_library(
    granit_xdg_shell_protocol OBJECT "${granit_xdg_shell_header}" "${granit_xdg_shell_source}"
  )
  set_target_properties(
    granit_xdg_shell_protocol
    PROPERTIES
      C_STANDARD 11
      C_STANDARD_REQUIRED YES
      C_VISIBILITY_PRESET hidden
      POSITION_INDEPENDENT_CODE YES
  )
  target_include_directories(
    granit_xdg_shell_protocol SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}"
                                           "${granit_wayland_protocol_dir}"
  )
endif()

add_subdirectory(src)

if(GRANIT_BUILD_TOOLS OR GRANIT_BUILD_EXAMPLES OR GRANIT_BUILD_BENCHMARKS)
  add_subdirectory(tools)
endif()

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(GRANIT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(GRANIT_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/granitConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/granitConfig.cmake"
  INSTALL_DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/granitConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/granitConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/granitConfigVersion.cmake"
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
)

install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
