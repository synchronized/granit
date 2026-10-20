# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 父项目或系统包优先；缺失时再获取锁定源码。测试框架不进入安装导出。
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
    target_include_directories(
      granit_unity
      SYSTEM PUBLIC "${GRANIT_UNITY_DIR}/src"
    )
    set_target_properties(
      granit_unity
      PROPERTIES
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
unset(_granit_unity_target)
