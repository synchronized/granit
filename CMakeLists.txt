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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(utility)

set(
  GRANIT_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/granit"
  CACHE STRING
  "Granit CMake 包配置安装目录"
)

add_subdirectory(src)

include(CTest)
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_subdirectory(3rd)
  add_subdirectory(tests)
endif()

if(GRANIT_BUILD_EXAMPLES)
  add_subdirectory(examples)
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
