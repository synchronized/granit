# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 父项目已经提供 Unity 目标时直接复用；否则尝试已安装包，最后使用内置源码。
set(_granit_unity_target "")

if(TARGET Unity::Unity)
  set(_granit_unity_target Unity::Unity)
  message(STATUS "Granit C tests reuse the existing Unity::Unity target")
elseif(TARGET unity::framework)
  set(_granit_unity_target unity::framework)
  message(STATUS "Granit C tests reuse the existing unity::framework target")
elseif(TARGET unity)
  set(_granit_unity_target unity)
  message(STATUS "Granit C tests reuse the existing unity target")
else()
  find_package(unity 2.6 CONFIG QUIET)

  if(TARGET Unity::Unity)
    set(_granit_unity_target Unity::Unity)
    message(STATUS "Granit C tests use Unity from find_package")
  elseif(TARGET unity::framework)
    set(_granit_unity_target unity::framework)
    message(STATUS "Granit C tests use unity::framework from find_package")
  elseif(TARGET unity)
    set(_granit_unity_target unity)
    message(STATUS "Granit C tests use unity from find_package")
  else()
    set(GRANIT_UNITY_DIR "${CMAKE_CURRENT_LIST_DIR}/unity-v2.6.1")
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
    message(STATUS "Granit C tests use bundled Unity 2.6.1")
  endif()
endif()

add_library(granit_test_unity INTERFACE)
target_link_libraries(granit_test_unity INTERFACE "${_granit_unity_target}")
if(NOT TARGET Unity::Unity)
  add_library(Unity::Unity ALIAS granit_test_unity)
endif()
unset(_granit_unity_target)
