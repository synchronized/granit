# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)
include(granit_integration_dependencies)

# 测试框架只参与测试构建，不进入 Granit 安装导出或生产库传递依赖。
# 父项目 target 和系统包优先，缺失时回退到 vendor/cache 中的锁定源码。
function(granit_prepare_catch2_test_dependency)
  granit_dependency_policy(CATCH2 granit_catch2_policy)

  if(NOT granit_catch2_policy STREQUAL "download" AND TARGET Catch2::Catch2WithMain)
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

  if(NOT granit_catch2_policy STREQUAL "download")
    find_package(Catch2 3 CONFIG QUIET)
  endif()
  if(NOT granit_catch2_policy STREQUAL "download" AND TARGET Catch2::Catch2WithMain)
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

  # 某些父项目只提供 Catch2::Catch2；此时仅补充 main，不能再次编译 Catch2 实现。
  if(NOT granit_catch2_policy STREQUAL "download" AND TARGET Catch2::Catch2)
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
    set_target_properties(granit_test_catch2_with_main PROPERTIES FOLDER "ThirdParty/Catch2")
    message(STATUS "Granit tests reuse the existing Catch2::Catch2 target")
    return()
  endif()

  set(GRANIT_CATCH2_DIR "")
  if(NOT granit_catch2_policy STREQUAL "system")
    granit_prepare_source_cache(
      "catch2" "${GRANIT_CATCH2_LOCKED_COMMIT}" "${GRANIT_CATCH2_LOCKED_URL}"
      "${GRANIT_CATCH2_LOCKED_SHA256}" "${GRANIT_CATCH2_LOCKED_MARKER}" GRANIT_CATCH2_DIR
    )
  elseif(NOT TARGET Catch2::Catch2)
    message(FATAL_ERROR "未找到 Catch2 3.x；当前获取策略为 ${granit_catch2_policy}")
  endif()

  if(NOT GRANIT_CATCH2_DIR)
    return()
  endif()

  set(CATCH_CONFIG_DEFAULT_REPORTER "console")
  set(CATCH_CONFIG_CONSOLE_WIDTH 80)
  set(granit_catch2_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/catch2-generated")
  configure_file(
    "${GRANIT_CATCH2_DIR}/src/catch2/catch_user_config.hpp.in"
    "${granit_catch2_generated_dir}/catch2/catch_user_config.hpp"
  )

  add_library(granit_test_catch2 INTERFACE)
  target_include_directories(
    granit_test_catch2 SYSTEM INTERFACE
      "${granit_catch2_generated_dir}"
      "${GRANIT_CATCH2_DIR}"
      "${GRANIT_CATCH2_DIR}/src"
      "${GRANIT_CATCH2_DIR}/extras"
  )
  target_compile_features(granit_test_catch2 INTERFACE cxx_std_20)
  if(NOT TARGET Catch2::Catch2)
    add_library(Catch2::Catch2 ALIAS granit_test_catch2)
  endif()

  add_library(
    granit_test_catch2_with_main STATIC "${GRANIT_CATCH2_DIR}/extras/catch_amalgamated.cpp"
  )
  target_compile_features(granit_test_catch2_with_main PUBLIC cxx_std_20)
  target_link_libraries(granit_test_catch2_with_main PUBLIC granit_test_catch2)
  if(NOT TARGET Catch2::Catch2WithMain)
    add_library(Catch2::Catch2WithMain ALIAS granit_test_catch2_with_main)
  endif()
  set_target_properties(granit_test_catch2_with_main PROPERTIES FOLDER "ThirdParty/Catch2")
  message(STATUS "Granit tests use cached Catch2 ${GRANIT_CATCH2_LOCKED_VERSION}")
endfunction()

function(granit_prepare_unity_test_dependency)
  granit_dependency_policy(UNITY granit_unity_policy)
  set(_granit_unity_target "")

  if(NOT granit_unity_policy STREQUAL "download" AND TARGET Unity::Unity)
    set(_granit_unity_target Unity::Unity)
    message(STATUS "Granit C tests reuse the existing Unity::Unity target")
  elseif(NOT granit_unity_policy STREQUAL "download" AND TARGET unity::framework)
    set(_granit_unity_target unity::framework)
    message(STATUS "Granit C tests reuse the existing unity::framework target")
  elseif(NOT granit_unity_policy STREQUAL "download" AND TARGET unity)
    set(_granit_unity_target unity)
    message(STATUS "Granit C tests reuse the existing unity target")
  else()
    if(NOT granit_unity_policy STREQUAL "download")
      find_package(unity 2.6 CONFIG QUIET)
    endif()

    if(NOT granit_unity_policy STREQUAL "download" AND TARGET Unity::Unity)
      set(_granit_unity_target Unity::Unity)
      message(STATUS "Granit C tests use Unity from find_package")
    elseif(NOT granit_unity_policy STREQUAL "download" AND TARGET unity::framework)
      set(_granit_unity_target unity::framework)
      message(STATUS "Granit C tests use unity::framework from find_package")
    elseif(NOT granit_unity_policy STREQUAL "download" AND TARGET unity)
      set(_granit_unity_target unity)
      message(STATUS "Granit C tests use unity from find_package")
    else()
      if(granit_unity_policy STREQUAL "system")
        message(FATAL_ERROR "未找到 Unity 2.6；当前获取策略为 system")
      endif()
      granit_prepare_source_cache(
        "unity" "${GRANIT_UNITY_LOCKED_COMMIT}" "${GRANIT_UNITY_LOCKED_URL}"
        "${GRANIT_UNITY_LOCKED_SHA256}" "${GRANIT_UNITY_LOCKED_MARKER}" GRANIT_UNITY_DIR
      )
      add_library(granit_unity STATIC "${GRANIT_UNITY_DIR}/src/unity.c")
      target_include_directories(granit_unity SYSTEM PUBLIC "${GRANIT_UNITY_DIR}/src")
      set_target_properties(
        granit_unity PROPERTIES
          C_STANDARD 11
          C_STANDARD_REQUIRED YES
          FOLDER "ThirdParty/Unity"
      )
      set(_granit_unity_target granit_unity)
      message(STATUS "Granit C tests use cached Unity ${GRANIT_UNITY_LOCKED_VERSION}")
    endif()
  endif()

  add_library(granit_test_unity INTERFACE)
  target_link_libraries(granit_test_unity INTERFACE "${_granit_unity_target}")
  if(NOT TARGET Unity::Unity)
    add_library(Unity::Unity ALIAS granit_test_unity)
  endif()
endfunction()

if(GRANIT_BUILD_TESTING)
  granit_prepare_unity_test_dependency()
  granit_prepare_catch2_test_dependency()
endif()
