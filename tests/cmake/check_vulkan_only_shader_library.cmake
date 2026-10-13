# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED ASSET_TOOL OR NOT DEFINED MANIFEST OR NOT DEFINED TOOLCHAIN
   OR NOT DEFINED CACHE_DIR OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "缺少 ASSET_TOOL、MANIFEST、TOOLCHAIN、CACHE_DIR 或 OUTPUT")
endif()

execute_process(
  COMMAND "${ASSET_TOOL}" shader build-library
    --manifest "${MANIFEST}"
    --toolchain "${TOOLCHAIN}"
    --cache "${CACHE_DIR}"
    --output "${OUTPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Vulkan-only Shader Library 构建失败 (code=${result})\n${stdout}\n${stderr}")
endif()

if(NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "未生成 Shader Library：${OUTPUT}")
endif()

file(GLOB_RECURSE wgsl_files "${CACHE_DIR}/*.wgsl")
if(wgsl_files)
  message(FATAL_ERROR "Vulkan-only Shader Library 不应生成 WGSL sidecar：${wgsl_files}")
endif()
