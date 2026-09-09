# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

# 库实现使用 WebGPU 头文件，最终程序链接对应的锁定 Port。
function(granit_target_webgpu target)
  target_compile_options(${target} PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false")
  target_link_options(${target} INTERFACE "--use-port=emdawnwebgpu:cpp_bindings=false")
endfunction()

# 配置浏览器可执行文件的共用页面属性；资源预加载和运行层能力由调用方声明。
function(granit_target_web_page target shell)
  target_link_options(${target} PRIVATE
    "--shell-file=${shell}"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
  )
  set_target_properties(${target} PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/web"
  )
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${shell}")
endfunction()
