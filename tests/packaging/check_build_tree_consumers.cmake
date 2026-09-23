# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR 和 GRANIT_TEST_BINARY_DIR")
endif()

foreach(mode IN ITEMS add_subdirectory fetch_content)
  set(consumer_binary_dir "${GRANIT_TEST_BINARY_DIR}/${mode}")
  file(REMOVE_RECURSE "${consumer_binary_dir}")
  set(
    configure_arguments
    -S "${GRANIT_SOURCE_DIR}/tests/packaging/build_tree"
    -B "${consumer_binary_dir}"
    "-DGRANIT_SOURCE_DIR=${GRANIT_SOURCE_DIR}"
    "-DGRANIT_CONSUMER_MODE=${mode}"
  )
  if(DEFINED GRANIT_TEST_GENERATOR AND NOT GRANIT_TEST_GENERATOR STREQUAL "")
    list(APPEND configure_arguments -G "${GRANIT_TEST_GENERATOR}")
  endif()
  if(DEFINED GRANIT_TEST_GENERATOR_PLATFORM AND NOT GRANIT_TEST_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND configure_arguments -A "${GRANIT_TEST_GENERATOR_PLATFORM}")
  endif()
  if(DEFINED GRANIT_TEST_GENERATOR_TOOLSET AND NOT GRANIT_TEST_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND configure_arguments -T "${GRANIT_TEST_GENERATOR_TOOLSET}")
  endif()
  if(DEFINED GRANIT_TEST_MAKE_PROGRAM AND NOT GRANIT_TEST_MAKE_PROGRAM STREQUAL "")
    list(APPEND configure_arguments "-DCMAKE_MAKE_PROGRAM=${GRANIT_TEST_MAKE_PROGRAM}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" ${configure_arguments}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${mode} 构建树消费检查失败\n${output}\n${error}")
  endif()
endforeach()

message(STATUS "add_subdirectory 与 FetchContent 构建树资产检查通过")
