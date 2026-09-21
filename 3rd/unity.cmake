# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 父项目已经提供 Unity 目标时直接复用；否则尝试已安装包，最后使用内置源码。
if(TARGET Unity::Unity)
  message(STATUS "Granit C tests reuse the existing Unity::Unity target")
elseif(TARGET unity::framework)
  add_library(Unity::Unity ALIAS unity::framework)
  message(STATUS "Granit C tests reuse the existing unity::framework target")
elseif(TARGET unity)
  add_library(Unity::Unity ALIAS unity)
  message(STATUS "Granit C tests reuse the existing unity target")
else()
  find_package(unity 2.6 CONFIG QUIET)

  if(TARGET Unity::Unity)
    message(STATUS "Granit C tests use Unity from find_package")
  elseif(TARGET unity::framework)
    add_library(Unity::Unity ALIAS unity::framework)
    message(STATUS "Granit C tests use unity::framework from find_package")
  elseif(TARGET unity)
    add_library(Unity::Unity ALIAS unity)
    message(STATUS "Granit C tests use unity from find_package")
  else()
    set(GRANIT_UNITY_DIR "${CMAKE_CURRENT_LIST_DIR}/unity-v2.6.1")
    add_library(granit_unity STATIC "${GRANIT_UNITY_DIR}/src/unity.c")
    add_library(Unity::Unity ALIAS granit_unity)
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
    message(STATUS "Granit C tests use bundled Unity 2.6.1")
  endif()
endif()
