# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "缺少 Runtime Bundle 测试参数")
endif()

set(test_root "${GRANIT_TEST_BINARY_DIR}/vulkan-runtime-bundle")
set(stage "${test_root}/stage")
set(output "${test_root}/runtime.zip")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${stage}/bin" "${stage}/validation" "${stage}/LICENSES")
file(WRITE "${stage}/bin/vulkan-1.dll" "test loader")
file(WRITE "${stage}/validation/VkLayer_khronos_validation.dll" "test validation layer")
file(WRITE "${stage}/validation/VkLayer_khronos_validation.json" "{}")
file(COPY "${GRANIT_SOURCE_DIR}/3rd/Vulkan-Headers-1.4.350/LICENSE.md"
     DESTINATION "${stage}/LICENSES")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSTAGE=${stage}"
    "-DOUTPUT=${output}"
    -DCOMPONENT=runtime
    -DPLATFORM=windows
    -DARCH=x64
    -DLOADER_VERSION=1.4.350
    -DVALIDATION_VERSION=1.4.350.0
    "-DLOADER_FILES:STRING=bin/vulkan-1.dll"
    "-DVALIDATION_FILES:STRING=validation/VkLayer_khronos_validation.dll;validation/VkLayer_khronos_validation.json"
    "-DLICENSE_FILES:STRING=LICENSES/LICENSE.md"
    -P "${GRANIT_SOURCE_DIR}/cmake/vulkan/package_vulkan_runtime.cmake"
  RESULT_VARIABLE package_result
)
if(NOT package_result EQUAL 0 OR NOT EXISTS "${output}")
  message(FATAL_ERROR "Runtime Bundle 测试归档失败")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${output}"
  OUTPUT_VARIABLE archive_listing
  ERROR_VARIABLE archive_error
  RESULT_VARIABLE listing_result
)
if(NOT listing_result EQUAL 0)
  message(FATAL_ERROR "Runtime Bundle 归档内容无法读取: ${archive_error}")
endif()
foreach(expected_path IN ITEMS
        "bin/vulkan-1.dll"
        "validation/VkLayer_khronos_validation.dll"
        "validation/VkLayer_khronos_validation.json"
        "vulkan-runtime.json")
  string(FIND "${archive_listing}" "${expected_path}" path_position)
  if(path_position LESS 0)
    message(FATAL_ERROR "Runtime Bundle 缺少归档文件: ${expected_path}\n${archive_listing}")
  endif()
endforeach()

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
