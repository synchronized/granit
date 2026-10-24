# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
set(fixture "${GRANIT_BINARY_DIR}/validation-runner-fixture")
file(MAKE_DIRECTORY "${fixture}")
file(WRITE "${fixture}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.23)
project(validation_fixture NONE)
enable_testing()
if(CASE STREQUAL "build_fail")
  add_custom_target(fail ALL COMMAND "${CMAKE_COMMAND}" -E false)
endif()
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
foreach(case IN ITEMS pass skip fail empty build_fail)
  # 先缓存用例模式，运行入口的重新配置应保留该模式。
  execute_process(COMMAND "${CMAKE_COMMAND}" --preset fixture "-DCASE=${case}"
                  WORKING_DIRECTORY "${fixture}" COMMAND_ERROR_IS_FATAL ANY)
  file(GLOB previous_reports "${fixture}/build/validation/*")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${fixture}"
    -DGRANIT_PRESET=fixture -DGRANIT_SUITE=quick
    -P "${GRANIT_SOURCE_DIR}/scripts/validate.cmake" RESULT_VARIABLE result)
  if((case STREQUAL "pass" OR case STREQUAL "skip") AND NOT result EQUAL 0)
    message(FATAL_ERROR "成功测试未通过验证入口")
  elseif((case STREQUAL "fail" OR case STREQUAL "empty" OR case STREQUAL "build_fail")
         AND result EQUAL 0)
    message(FATAL_ERROR "${case} 被错误标为成功")
  endif()
  file(GLOB reports "${fixture}/build/validation/*")
  list(REMOVE_ITEM reports ${previous_reports})
  list(LENGTH reports report_count)
  if(NOT report_count EQUAL 1)
    message(FATAL_ERROR "每次运行必须恰好生成一份报告")
  endif()
  list(GET reports 0 report)
  file(READ "${report}/summary.json" summary)
  string(JSON status GET "${summary}" status)
  if(case STREQUAL "skip" AND NOT status STREQUAL "incomplete")
    message(FATAL_ERROR "跳过必须报告为 incomplete")
  endif()
endforeach()
