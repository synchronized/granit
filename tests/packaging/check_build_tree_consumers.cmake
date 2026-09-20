# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR 和 GRANIT_TEST_BINARY_DIR")
endif()

foreach(mode IN ITEMS add_subdirectory fetch_content)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${GRANIT_SOURCE_DIR}/tests/packaging/build_tree"
      -B "${GRANIT_TEST_BINARY_DIR}/${mode}" "-DGRANIT_SOURCE_DIR=${GRANIT_SOURCE_DIR}"
      "-DGRANIT_CONSUMER_MODE=${mode}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${mode} 构建树消费检查失败\n${output}\n${error}")
  endif()
endforeach()

message(STATUS "add_subdirectory 与 FetchContent 构建树资产检查通过")
