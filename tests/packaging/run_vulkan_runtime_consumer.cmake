# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)

foreach(required IN ITEMS
        GRANIT_CONSUMER_BINARY_DIR
        GRANIT_INSTALL_PREFIX
        GRANIT_RUNTIME_ROOT
        GRANIT_RUNTIME_LOADER)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "缺少 Runtime Consumer 参数：${required}")
  endif()
endforeach()

if(NOT DEFINED GRANIT_TEST_CONFIGURATION OR "${GRANIT_TEST_CONFIGURATION}" STREQUAL "")
  set(GRANIT_TEST_CONFIGURATION Release)
endif()

set(consumer_suffix "")
if(WIN32)
  set(consumer_suffix ".exe")
  set(runtime_path_variable "PATH")
  set(runtime_path_value
      "${GRANIT_RUNTIME_ROOT};${GRANIT_INSTALL_PREFIX}/bin;$ENV{PATH}")
elseif(APPLE)
  set(runtime_path_variable "DYLD_LIBRARY_PATH")
  set(runtime_path_value
      "${GRANIT_RUNTIME_ROOT};${GRANIT_INSTALL_PREFIX}/lib;$ENV{DYLD_LIBRARY_PATH}")
else()
  set(runtime_path_variable "LD_LIBRARY_PATH")
  set(runtime_path_value
      "${GRANIT_RUNTIME_ROOT};${GRANIT_INSTALL_PREFIX}/lib;$ENV{LD_LIBRARY_PATH}")
endif()

foreach(consumer IN ITEMS granit_c_consumer granit_cpp_consumer)
  set(binary "${GRANIT_CONSUMER_BINARY_DIR}/${consumer}${consumer_suffix}")
  if(NOT EXISTS "${binary}")
    message(FATAL_ERROR "找不到安装 Consumer：${binary}")
  endif()

  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "GRANIT_VULKAN_RUNTIME=bundled"
      "GRANIT_VULKAN_LOADER_PATH=${GRANIT_RUNTIME_LOADER}"
      "${runtime_path_variable}=${runtime_path_value}"
      "${binary}"
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Runtime Bundle Consumer 运行失败：${consumer}（${result}）")
  endif()
endforeach()
