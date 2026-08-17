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
option(GRANIT_ENABLE_XCB "在 Linux 上启用 XCB Surface 与 Window 后端" ON)
option(GRANIT_ENABLE_WAYLAND "在 Linux 上启用 Wayland Surface 后端" ON)

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
      message(STATUS "Granit Wayland xdg-shell tests enabled")
    else()
      message(STATUS "Granit Wayland xdg-shell tests disabled: protocol tools not found")
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

include(CTest)
add_subdirectory(3rd)
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
