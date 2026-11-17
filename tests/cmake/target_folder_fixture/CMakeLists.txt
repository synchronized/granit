# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_target_folder_fixture LANGUAGES NONE)

include("${GRANIT_UTILITY}")

add_library(granit_foldered INTERFACE)
set_target_properties(granit_foldered PROPERTIES FOLDER "Tests/Smoke")
add_library(granit_unfoldered INTERFACE)
add_library(external_foldered INTERFACE)
set_target_properties(external_foldered PROPERTIES FOLDER "External")

granit_apply_cmake_target_folder("${CMAKE_CURRENT_SOURCE_DIR}")

function(assert_folder target expected)
  get_property(actual TARGET "${target}" PROPERTY FOLDER)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "${target} 的 FOLDER='${actual}'，期望 '${expected}'")
  endif()
endfunction()

if(EXPECTED_PREFIX STREQUAL "")
  assert_folder(granit_foldered "Tests/Smoke")
  get_property(unfoldered_folder TARGET granit_unfoldered PROPERTY FOLDER)
  if(unfoldered_folder AND NOT unfoldered_folder MATCHES "-NOTFOUND$")
    message(FATAL_ERROR "默认配置不应写入未归类目标的 FOLDER")
  endif()
else()
  assert_folder(granit_foldered "${EXPECTED_PREFIX}/Tests/Smoke")
  assert_folder(granit_unfoldered "${EXPECTED_PREFIX}")
endif()

assert_folder(external_foldered "External")
