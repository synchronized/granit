# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)

foreach(required STAGE OUTPUT COMPONENT PLATFORM ARCH LOADER_VERSION LOADER_FILES LICENSE_FILES)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "缺少 Runtime Bundle 参数：${required}")
  endif()
endforeach()

if(NOT DEFINED ARCHIVE_FORMAT OR "${ARCHIVE_FORMAT}" STREQUAL "")
  set(ARCHIVE_FORMAT zip)
endif()
if(NOT ARCHIVE_FORMAT STREQUAL "zip" AND NOT ARCHIVE_FORMAT STREQUAL "gnutar")
  message(FATAL_ERROR "不支持的 Runtime Bundle 归档格式：${ARCHIVE_FORMAT}")
endif()

set(GENERATED_MANIFEST "${STAGE}/vulkan-runtime.json")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSTAGE=${STAGE}"
    "-DOUTPUT=${GENERATED_MANIFEST}"
    "-DCOMPONENT=${COMPONENT}"
    "-DPLATFORM=${PLATFORM}"
    "-DARCH=${ARCH}"
    "-DLOADER_VERSION=${LOADER_VERSION}"
    "-DLOADER_FILES:STRING=${LOADER_FILES}"
    "-DLICENSE_FILES:STRING=${LICENSE_FILES}"
    "-DVALIDATION_FILES:STRING=${VALIDATION_FILES}"
    "-DVALIDATION_VERSION:STRING=${VALIDATION_VERSION}"
    -P "${CMAKE_CURRENT_LIST_DIR}/generate_vulkan_runtime_manifest.cmake"
  RESULT_VARIABLE generate_result
)
if(NOT generate_result EQUAL 0)
  message(FATAL_ERROR "Runtime Bundle manifest 生成失败")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSTAGE=${STAGE}"
    "-DMANIFEST=${GENERATED_MANIFEST}"
    -P "${CMAKE_CURRENT_LIST_DIR}/verify_vulkan_runtime_manifest.cmake"
  RESULT_VARIABLE verify_result
)
if(NOT verify_result EQUAL 0)
  message(FATAL_ERROR "Runtime Bundle manifest 校验失败")
endif()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT}" --format=${ARCHIVE_FORMAT} .
  WORKING_DIRECTORY "${STAGE}"
  RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
  message(FATAL_ERROR "Runtime Bundle 归档失败：${OUTPUT}")
endif()

message(STATUS "Runtime Bundle 已生成：${OUTPUT}")
