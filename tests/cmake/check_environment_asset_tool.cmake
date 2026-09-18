# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED TOOL OR NOT DEFINED WORK_DIR OR NOT DEFINED FIXTURE)
  message(FATAL_ERROR "必须提供 TOOL、WORK_DIR 和 FIXTURE")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
string(REPEAT "I" 48 irradiance_pixels)
string(REPEAT "P" 48 prefiltered_pixels)
string(REPEAT "B" 8 brdf_pixels)
file(WRITE "${WORK_DIR}/irradiance.bin" "${irradiance_pixels}")
file(WRITE "${WORK_DIR}/prefiltered.bin" "${prefiltered_pixels}")
file(WRITE "${WORK_DIR}/brdf.bin" "${brdf_pixels}")
execute_process(
  COMMAND
    "${TOOL}" environment build --irradiance "${WORK_DIR}/irradiance.bin"
    --irradiance-resolution 1 --prefiltered "1=${WORK_DIR}/prefiltered.bin" --brdf-lut
    "${WORK_DIR}/brdf.bin" --brdf-width 1 --brdf-height 1 --output "${WORK_DIR}/built.grenv"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0 OR NOT EXISTS "${WORK_DIR}/built.grenv")
  message(FATAL_ERROR "Environment build 失败：${build_output}${build_error}")
endif()
execute_process(
  COMMAND "${TOOL}" environment inspect "${FIXTURE}" --json --output "${WORK_DIR}/debug.json"
  RESULT_VARIABLE inspect_result
  OUTPUT_VARIABLE inspect_output
  ERROR_VARIABLE inspect_error
)
if(NOT inspect_result EQUAL 0 OR NOT EXISTS "${WORK_DIR}/debug.json")
  message(FATAL_ERROR "Environment inspect 失败：${inspect_output}${inspect_error}")
endif()
execute_process(
  COMMAND "${TOOL}" environment inspect "${WORK_DIR}/built.grenv" --json
  RESULT_VARIABLE built_inspect_result
  OUTPUT_VARIABLE built_debug_json
  ERROR_VARIABLE built_inspect_error
)
if(NOT built_inspect_result EQUAL 0 OR NOT built_debug_json MATCHES "GRENV03")
  message(FATAL_ERROR "新构建 Environment 检查失败：${built_inspect_error}")
endif()
file(READ "${WORK_DIR}/debug.json" debug_json)
if(NOT debug_json MATCHES "\\\"magic\\\": \\\"GRENV03\\\"")
  message(FATAL_ERROR "Environment inspect 未生成预期调试 JSON")
endif()
