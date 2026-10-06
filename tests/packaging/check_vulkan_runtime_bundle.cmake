# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "缺少 Runtime Bundle 测试参数")
endif()

set(test_root "${GRANIT_TEST_BINARY_DIR}/vulkan-runtime-bundle")
set(stage "${test_root}/stage")
set(output "${test_root}/runtime.zip")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${stage}/bin" "${stage}/LICENSES")
file(WRITE "${stage}/bin/vulkan-1.dll" "test loader")
file(COPY "${GRANIT_SOURCE_DIR}/3rd/Vulkan-Headers-1.4.350/LICENSE.md"
     DESTINATION "${stage}/LICENSES")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSTAGE=${stage}"
    "-DOUTPUT=${output}"
    -DCOMPONENT=loader
    -DPLATFORM=windows
    -DARCH=x64
    -DLOADER_VERSION=1.4.350
    "-DLOADER_FILES:STRING=bin/vulkan-1.dll"
    "-DLICENSE_FILES:STRING=LICENSES/LICENSE.md"
    -P "${GRANIT_SOURCE_DIR}/cmake/vulkan/package_vulkan_runtime.cmake"
  RESULT_VARIABLE package_result
)
if(NOT package_result EQUAL 0 OR NOT EXISTS "${output}")
  message(FATAL_ERROR "Runtime Bundle 测试归档失败")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSTAGE=${stage}"
    "-DMANIFEST=${stage}/vulkan-runtime.json"
    -P "${GRANIT_SOURCE_DIR}/cmake/vulkan/verify_vulkan_runtime_manifest.cmake"
  RESULT_VARIABLE verify_result
)
if(NOT verify_result EQUAL 0)
  message(FATAL_ERROR "Runtime Bundle 测试 manifest 校验失败")
endif()

file(REMOVE_RECURSE "${test_root}")
