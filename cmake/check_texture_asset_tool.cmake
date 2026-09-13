# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED TOOL OR NOT DEFINED WORK_DIR)
  message(FATAL_ERROR "必须提供 TOOL 和 WORK_DIR")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(input "${WORK_DIR}/rgba8.bin")
set(manifest "${WORK_DIR}/texture.grtex")
set(payload "${WORK_DIR}/texture.bin")
file(WRITE "${input}" "0000000000000000000000000000000000000000000000000000000000000000")
execute_process(
  COMMAND
    "${TOOL}" texture build --output "${manifest}" --payload-output "${payload}" --dimension
    2d --width 4 --height 4 --depth 1 --layers 1 --mips 1 --variant
    "rgba8-srgb=${input}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Texture CLI 构建失败：\n${build_output}\n${build_error}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${input}" "${payload}"
  RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
  message(FATAL_ERROR "Texture CLI 输出负载与输入不一致")
endif()
execute_process(
  COMMAND "${TOOL}" texture inspect "${manifest}" --json
  RESULT_VARIABLE inspect_result
  OUTPUT_VARIABLE inspect_output
  ERROR_VARIABLE inspect_error
)
if(NOT inspect_result EQUAL 0 OR NOT inspect_output MATCHES "\"content_id\"")
  message(FATAL_ERROR "Texture CLI 检查失败：\n${inspect_output}\n${inspect_error}")
endif()
