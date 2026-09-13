# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED TOOL OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR
   NOT DEFINED ASSET_TOOLS_LIBRARY)
  message(FATAL_ERROR "缺少 Shader 工具诊断测试参数")
endif()

file(REMOVE "${OUTPUT}")
set(wgsl_output "${OUTPUT}.wgsl")
file(REMOVE "${wgsl_output}")
set(toolchain_root "${OUTPUT}.toolchain")
file(MAKE_DIRECTORY "${toolchain_root}/bin")
if(WIN32)
  set(dxc_name dxc.exe)
  set(tint_name tint.exe)
else()
  set(dxc_name dxc)
  set(tint_name tint)
endif()
file(COPY_FILE "${TOOL}" "${toolchain_root}/bin/${dxc_name}" ONLY_IF_DIFFERENT)
file(COPY_FILE "${TOOL}" "${toolchain_root}/bin/${tint_name}" ONLY_IF_DIFFERENT)
file(COPY "${ASSET_TOOLS_LIBRARY}" DESTINATION "${toolchain_root}/bin")
execute_process(
  COMMAND
    "${TOOL}" shader compile --toolchain "${toolchain_root}" --input "${INPUT}"
    --entry vertex_main --stage vertex --spirv-output "${OUTPUT}" --wgsl-output "${wgsl_output}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE standard_output
  ERROR_VARIABLE standard_error
)
if(NOT result EQUAL 1)
  message(FATAL_ERROR "Shader 工具应返回编译失败，实际退出码：${result}")
endif()
# Windows CMake 捕获 UTF-8 子进程输出时可能按活动代码页显示；只匹配稳定 ASCII 事实。
if(NOT standard_error MATCHES "granit_asset_tool shader inspect" OR
   NOT standard_error MATCHES "DXC")
  message(FATAL_ERROR "未捕获完整 DXC 子进程诊断：${standard_error}")
endif()
if(EXISTS "${OUTPUT}" OR EXISTS "${wgsl_output}")
  message(FATAL_ERROR "DXC 编译失败后仍保留输出文件")
endif()
