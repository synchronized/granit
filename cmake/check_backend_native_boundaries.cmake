# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "check_backend_native_boundaries.cmake 需要 SOURCE_DIR")
endif()

file(
  GLOB_RECURSE source_files
  LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/src/*.c"
  "${SOURCE_DIR}/src/*.cc"
  "${SOURCE_DIR}/src/*.cpp"
  "${SOURCE_DIR}/src/*.h"
  "${SOURCE_DIR}/src/*.hpp"
)

foreach(source_file IN LISTS source_files)
  file(TO_CMAKE_PATH "${source_file}" normalized_file)
  if(normalized_file MATCHES "/src/backend/")
    continue()
  endif()
  file(READ "${source_file}" content)
  if(content MATCHES "(^|[^A-Za-z0-9_])(WGPU[A-Za-z0-9_]*|wgpu[A-Za-z0-9_]+)")
    message(FATAL_ERROR "WebGPU 原生符号越过 Backend 边界：${source_file}")
  endif()
endforeach()
