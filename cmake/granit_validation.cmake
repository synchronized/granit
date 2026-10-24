# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 配置时记录实际工具链，不把 Runner 名称当作编译器身份。
set(granit_environment "{}")
foreach(key IN ITEMS CMAKE_C_COMPILER CMAKE_C_COMPILER_ID CMAKE_C_COMPILER_VERSION
                     CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ID CMAKE_CXX_COMPILER_VERSION
                     CMAKE_SYSTEM_NAME CMAKE_SYSTEM_PROCESSOR CMAKE_BUILD_TYPE
                     BUILD_SHARED_LIBS GRANIT_PROJECT_VERSION GRANIT_SANITIZER)
  set(value "${${key}}")
  string(REPLACE "\\" "/" value "${value}")
  string(REPLACE "\"" "\\\"" value "${value}")
  string(JSON granit_environment SET "${granit_environment}" "${key}" "\"${value}\"")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/validation-environment.json" "${granit_environment}\n")
