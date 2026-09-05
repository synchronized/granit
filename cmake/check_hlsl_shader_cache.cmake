# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required TOOL DXC TINT INPUT OUTPUT_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(spirv "${OUTPUT_DIR}/hlsl-cli.spv")
set(wgsl "${OUTPUT_DIR}/hlsl-cli.wgsl")
set(asset "${OUTPUT_DIR}/hlsl-cli.granit-shader")
set(common_arguments
    --input "${INPUT}" --entry fragment_main --stage fragment --spirv-output "${spirv}"
    --wgsl-output "${wgsl}" --asset "${asset}" --dxc-revision test-dxc --tint-revision
    test-tint --asset-backend all)

execute_process(
  COMMAND "${TOOL}" compile-hlsl --dxc "${DXC}" --tint "${TINT}" ${common_arguments}
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0 OR NOT EXISTS "${spirv}" OR NOT EXISTS "${wgsl}")
  message(FATAL_ERROR "HLSL 资产首次生成失败：${build_output}${build_error}")
endif()

file(REMOVE "${spirv}" "${wgsl}")
execute_process(
  COMMAND "${TOOL}" compile-hlsl --dxc missing-dxc --tint missing-tint ${common_arguments}
  RESULT_VARIABLE restore_result
  OUTPUT_VARIABLE restore_output
  ERROR_VARIABLE restore_error
)
if(NOT restore_result EQUAL 0 OR NOT EXISTS "${spirv}" OR NOT EXISTS "${wgsl}")
  message(FATAL_ERROR "HLSL 资产未在启动编译器前恢复：${restore_output}${restore_error}")
endif()
