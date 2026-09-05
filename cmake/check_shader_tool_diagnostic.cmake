# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED TOOL OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "缺少 Shader 工具诊断测试参数")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
  COMMAND
    "${TOOL}" compile --tint "${TOOL}" --input "${INPUT}" --entry vs_main --stage vertex
    --output "${OUTPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE standard_output
  ERROR_VARIABLE standard_error
)
if(NOT result EQUAL 1)
  message(FATAL_ERROR "Shader 工具应返回编译失败，实际退出码：${result}")
endif()
# Windows CMake 捕获 UTF-8 子进程输出时可能按活动代码页显示；只匹配稳定 ASCII 事实。
if(NOT standard_error MATCHES "granit_shader_tool inspect" OR
   NOT standard_error MATCHES "Tint")
  message(FATAL_ERROR "未捕获完整 Tint 子进程诊断：${standard_error}")
endif()
if(EXISTS "${OUTPUT}")
  message(FATAL_ERROR "Tint 编译失败后仍保留输出文件")
endif()
