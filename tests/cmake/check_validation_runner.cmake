# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
set(fixture "${GRANIT_BINARY_DIR}/validation-runner-fixture")
file(MAKE_DIRECTORY "${fixture}")
file(WRITE "${fixture}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.23)
project(validation_fixture NONE)
enable_testing()
if(NOT CASE STREQUAL "empty")
  if(CASE STREQUAL "skip")
    add_test(NAME fixture COMMAND "${CMAKE_COMMAND}" -E true)
    set_tests_properties(fixture PROPERTIES SKIP_RETURN_CODE 0)
  elseif(CASE STREQUAL "fail")
    add_test(NAME fixture COMMAND "${CMAKE_COMMAND}" -E false)
  else()
    add_test(NAME fixture COMMAND "${CMAKE_COMMAND}" -E true)
  endif()
  set_tests_properties(fixture PROPERTIES LABELS unit)
endif()
]=])
file(WRITE "${fixture}/CMakePresets.json" [=[
{"version":4,
 "configurePresets":[{"name":"fixture","generator":"Ninja","binaryDir":"${sourceDir}/build/fixture"}],
 "buildPresets":[{"name":"fixture","configurePreset":"fixture"}],
 "testPresets":[{"name":"fixture","configurePreset":"fixture"}]}
]=])
foreach(case IN ITEMS pass skip fail empty)
  # 先缓存用例模式，运行入口的重新配置应保留该模式。
  execute_process(COMMAND "${CMAKE_COMMAND}" --preset fixture "-DCASE=${case}"
                  WORKING_DIRECTORY "${fixture}" COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${fixture}"
    -DGRANIT_PRESET=fixture -DGRANIT_SUITE=quick
    -P "${GRANIT_SOURCE_DIR}/scripts/validate.cmake" RESULT_VARIABLE result)
  if((case STREQUAL "pass" OR case STREQUAL "skip") AND NOT result EQUAL 0)
    message(FATAL_ERROR "成功测试未通过验证入口")
  elseif((case STREQUAL "fail" OR case STREQUAL "empty") AND result EQUAL 0)
    message(FATAL_ERROR "${case} 被错误标为成功")
  endif()
endforeach()
