# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required TOOL GLSLANG TINT INPUT OUTPUT_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(spirv "${OUTPUT_DIR}/material.spv")
set(wgsl "${OUTPUT_DIR}/material.wgsl")
set(asset "${OUTPUT_DIR}/material.granit-shader")
set(common_arguments
    --input "${INPUT}" --entry main --stage fragment --spirv-output "${spirv}"
    --wgsl-output "${wgsl}" --asset "${asset}" --glslang-revision test-glslang
    --tint-revision test-tint --asset-backend all)

execute_process(
  COMMAND "${TOOL}" compile-glsl --glslang "${GLSLANG}" --tint "${TINT}" ${common_arguments}
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "GLSL 资产首次生成失败：${build_output}${build_error}")
endif()

execute_process(
  COMMAND
    "${TOOL}" compile-glsl --glslang "${GLSLANG}" --tint "${TINT}" --input "${INPUT}"
    --entry main --stage fragment --spirv-output "${OUTPUT_DIR}/auto.spv" --wgsl-output
    "${OUTPUT_DIR}/auto.wgsl" --asset "${OUTPUT_DIR}/auto.granit-shader"
  RESULT_VARIABLE automatic_identity_result
  OUTPUT_VARIABLE automatic_identity_output
  ERROR_VARIABLE automatic_identity_error
)
if(NOT automatic_identity_result EQUAL 0)
  message(
    FATAL_ERROR
    "GLSL 资产无法自动记录工具身份：${automatic_identity_output}${automatic_identity_error}"
  )
endif()

file(REMOVE "${spirv}" "${wgsl}")
execute_process(
  COMMAND
    "${TOOL}" compile-glsl --glslang missing-glslang --tint missing-tint ${common_arguments}
  RESULT_VARIABLE restore_result
  OUTPUT_VARIABLE restore_output
  ERROR_VARIABLE restore_error
)
if(NOT restore_result EQUAL 0 OR NOT EXISTS "${spirv}" OR NOT EXISTS "${wgsl}")
  message(FATAL_ERROR "GLSL 资产未在启动编译器前恢复：${restore_output}${restore_error}")
endif()
