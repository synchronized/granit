# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 父项目已经提供完整 Catch2 目标时直接复用，避免重复编译测试框架。
if(TARGET Catch2::Catch2WithMain)
  if(DEFINED Catch2_VERSION AND Catch2_VERSION VERSION_LESS 3.0)
    message(
      FATAL_ERROR
      "Granit tests require Catch2 3.x, but the existing target is Catch2 ${Catch2_VERSION}"
    )
  endif()
  add_library(granit_test_catch2_with_main INTERFACE)
  target_link_libraries(granit_test_catch2_with_main INTERFACE Catch2::Catch2WithMain)
  message(STATUS "Granit tests reuse the existing Catch2::Catch2WithMain target")
  return()
endif()

# 其次尝试使用系统或父项目包管理器提供的 Catch2 3。
find_package(Catch2 3 CONFIG QUIET)
if(TARGET Catch2::Catch2WithMain)
  if(DEFINED Catch2_VERSION AND Catch2_VERSION VERSION_LESS 3.0)
    message(
      FATAL_ERROR
      "Granit tests require Catch2 3.x, but the discovered package is Catch2 ${Catch2_VERSION}"
    )
  endif()
  add_library(granit_test_catch2_with_main INTERFACE)
  target_link_libraries(granit_test_catch2_with_main INTERFACE Catch2::Catch2WithMain)
  message(STATUS "Granit tests use Catch2 from find_package")
  return()
endif()

set(GRANIT_CATCH2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/catch-v3.15.0")

# 某些父项目只提供 Catch2::Catch2；此时仅补充 main，不能再次编译 Catch2 实现。
if(TARGET Catch2::Catch2)
  if(DEFINED Catch2_VERSION AND Catch2_VERSION VERSION_LESS 3.0)
    message(
      FATAL_ERROR
      "Granit tests require Catch2 3.x, but the existing target is Catch2 ${Catch2_VERSION}"
    )
  endif()
  add_library(
    granit_test_catch2_with_main
    STATIC "${PROJECT_SOURCE_DIR}/tests/support/catch_main.cpp"
  )
  target_compile_features(granit_test_catch2_with_main PUBLIC cxx_std_20)
  target_link_libraries(granit_test_catch2_with_main PUBLIC Catch2::Catch2)
  add_library(Catch2::Catch2WithMain ALIAS granit_test_catch2_with_main)
  set_target_properties(
    granit_test_catch2_with_main
    PROPERTIES FOLDER "ThirdParty/Catch2"
  )
  message(STATUS "Granit tests reuse the existing Catch2::Catch2 target")
  return()
endif()

add_library(granit_test_catch2 INTERFACE)
target_include_directories(
  granit_test_catch2
  SYSTEM INTERFACE "${GRANIT_CATCH2_DIR}"
)
target_compile_features(granit_test_catch2 INTERFACE cxx_std_20)
add_library(Catch2::Catch2 ALIAS granit_test_catch2)

add_library(
  granit_test_catch2_with_main
  STATIC "${GRANIT_CATCH2_DIR}/catch2/catch_amalgamated.cpp"
)
target_compile_features(granit_test_catch2_with_main PUBLIC cxx_std_20)
target_link_libraries(granit_test_catch2_with_main PUBLIC granit_test_catch2)
add_library(Catch2::Catch2WithMain ALIAS granit_test_catch2_with_main)
set_target_properties(
  granit_test_catch2_with_main
  PROPERTIES FOLDER "ThirdParty/Catch2"
)

message(STATUS "Granit tests use bundled Catch2 3.15.0")
