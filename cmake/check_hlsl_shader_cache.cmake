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
set(asset "${OUTPUT_DIR}/hlsl-cli.grshaderobj")
set(common_arguments
    --input "${INPUT}" --entry fragment_main --stage fragment --spirv-output "${spirv}"
    --wgsl-output "${wgsl}" --object "${asset}" --dxc-revision test-dxc --tint-revision
    test-tint --object-backend all --define SECOND_VALUE=2 --define FIRST_VALUE=1)

execute_process(
  COMMAND "${TOOL}" compile --dxc "${DXC}" --tint "${TINT}" ${common_arguments}
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0 OR NOT EXISTS "${spirv}" OR NOT EXISTS "${wgsl}")
  message(FATAL_ERROR "HLSL 资产首次生成失败：${build_output}${build_error}")
endif()

file(REMOVE "${spirv}" "${wgsl}")
execute_process(
  COMMAND "${TOOL}" compile --dxc missing-dxc --tint missing-tint ${common_arguments}
  RESULT_VARIABLE restore_result
  OUTPUT_VARIABLE restore_output
  ERROR_VARIABLE restore_error
)
if(NOT restore_result EQUAL 0 OR NOT EXISTS "${spirv}" OR NOT EXISTS "${wgsl}")
  message(FATAL_ERROR "HLSL 资产未在启动编译器前恢复：${restore_output}${restore_error}")
endif()

execute_process(
  COMMAND "${TOOL}" compile --dxc missing-dxc --tint missing-tint
          ${common_arguments} --define THIRD_VALUE=3
  RESULT_VARIABLE changed_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(changed_result EQUAL 0)
  message(FATAL_ERROR "HLSL 宏定义变化后错误命中旧缓存")
endif()
