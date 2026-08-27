# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_INSTALL_PREFIX OR
   NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR、GRANIT_INSTALL_PREFIX 和 GRANIT_TEST_BINARY_DIR")
endif()

function(granit_check_package name expected_success)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}"
      -S "${GRANIT_SOURCE_DIR}/tests/package"
      -B "${GRANIT_TEST_BINARY_DIR}/${name}"
      "-DCMAKE_PREFIX_PATH=${GRANIT_INSTALL_PREFIX}"
      ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(expected_success AND NOT result EQUAL 0)
    message(FATAL_ERROR "${name} 应配置成功，但返回 ${result}\n${output}\n${error}")
  endif()
  if(NOT expected_success AND result EQUAL 0)
    message(FATAL_ERROR "${name} 应配置失败，但意外成功")
  endif()
endfunction()

granit_check_package(compatible TRUE -DGRANIT_REQUEST_VERSION=0.3)
granit_check_package(compatible_older_minor TRUE -DGRANIT_REQUEST_VERSION=0.2)
granit_check_package(compatible_older_minor TRUE -DGRANIT_REQUEST_VERSION=0.1)
granit_check_package(exact TRUE -DGRANIT_REQUEST_VERSION=0.4.0 -DGRANIT_REQUEST_EXACT=ON)
granit_check_package(newer_minor FALSE -DGRANIT_REQUEST_VERSION=0.4)
granit_check_package(incompatible_major FALSE -DGRANIT_REQUEST_VERSION=1.0)
granit_check_package(unknown_component FALSE -DGRANIT_REQUEST_COMPONENT=Unknown)

message(STATUS "安装包选包检查通过：兼容与精确版本成功，错误主版本和未知 component 被拒绝")
