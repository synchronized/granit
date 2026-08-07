# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(GRANIT_VULKAN_HEADERS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan-Headers-1.4.350/include")
set(GRANIT_VOLK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/volk-1.4.350")

# Volk 作为内部对象并入 Granit，避免静态安装包导出私有第三方 target。
add_library(granit_volk OBJECT "${GRANIT_VOLK_DIR}/volk.c")
set_source_files_properties("${GRANIT_VOLK_DIR}/volk.c" PROPERTIES LANGUAGE CXX)
target_compile_features(granit_volk PRIVATE cxx_std_20)
target_compile_definitions(granit_volk PRIVATE VOLK_NAMESPACE)
target_include_directories(
  granit_volk
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)
set_target_properties(
  granit_volk
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
    VISIBILITY_INLINES_HIDDEN YES
    FOLDER "ThirdParty/Volk"
)

set(GRANIT_VULKAN_HEADERS_DIR "${GRANIT_VULKAN_HEADERS_DIR}" PARENT_SCOPE)
set(GRANIT_VOLK_DIR "${GRANIT_VOLK_DIR}" PARENT_SCOPE)

if(NOT GRANIT_BUILD_TESTING OR NOT BUILD_TESTING)
  return()
endif()

# 父项目已经提供完整 Catch2 目标时直接复用，避免重复编译测试框架。
if(TARGET Catch2::Catch2WithMain)
  message(STATUS "Granit tests reuse the existing Catch2::Catch2WithMain target")
  return()
endif()

# 其次尝试使用系统或父项目包管理器提供的 Catch2 3。
find_package(Catch2 3 CONFIG QUIET)
if(TARGET Catch2::Catch2WithMain)
  message(STATUS "Granit tests use Catch2 from find_package")
  return()
endif()

set(GRANIT_CATCH2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/catch-v3.15.0")

# 某些父项目只提供 Catch2::Catch2；此时仅补充 main，不能再次编译 Catch2 实现。
if(TARGET Catch2::Catch2)
  add_library(
    granit_catch2_with_main
    STATIC "${PROJECT_SOURCE_DIR}/tests/support/catch_main.cpp"
  )
  add_library(Catch2::Catch2WithMain ALIAS granit_catch2_with_main)
  target_compile_features(granit_catch2_with_main PUBLIC cxx_std_20)
  target_link_libraries(granit_catch2_with_main PUBLIC Catch2::Catch2)
  set_target_properties(
    granit_catch2_with_main
    PROPERTIES FOLDER "ThirdParty/Catch2"
  )
  message(STATUS "Granit tests reuse the existing Catch2::Catch2 target")
  return()
endif()

add_library(granit_catch2 INTERFACE)
add_library(Catch2::Catch2 ALIAS granit_catch2)
target_include_directories(
  granit_catch2
  SYSTEM INTERFACE "${GRANIT_CATCH2_DIR}"
)
target_compile_features(granit_catch2 INTERFACE cxx_std_20)

add_library(
  granit_catch2_with_main
  STATIC "${GRANIT_CATCH2_DIR}/catch2/catch_amalgamated.cpp"
)
add_library(Catch2::Catch2WithMain ALIAS granit_catch2_with_main)
target_compile_features(granit_catch2_with_main PUBLIC cxx_std_20)
target_link_libraries(granit_catch2_with_main PUBLIC Catch2::Catch2)
set_target_properties(
  granit_catch2_with_main
  PROPERTIES FOLDER "ThirdParty/Catch2"
)

message(STATUS "Granit tests use bundled Catch2 3.15.0")
