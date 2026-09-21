# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED DESTINATION OR DESTINATION STREQUAL "")
  message(FATAL_ERROR "必须通过 DESTINATION 指定工具链缓存目录")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/GranitShaderToolchainLock.cmake")
if(NOT GRANIT_SHADER_TOOLCHAIN_PACKAGE_NAME)
  message(FATAL_ERROR "当前仅提供 Windows x64 与 Linux x64 Shader 工具链包")
endif()
set(package_name "${GRANIT_SHADER_TOOLCHAIN_PACKAGE_NAME}")
set(archive_name "${GRANIT_SHADER_TOOLCHAIN_ARCHIVE_NAME}")
set(archive_sha256 "${GRANIT_SHADER_TOOLCHAIN_ARCHIVE_SHA256}")

cmake_path(ABSOLUTE_PATH DESTINATION NORMALIZE OUTPUT_VARIABLE destination_absolute)
set(toolchain_root "${destination_absolute}/${package_name}")
set(manifest "${toolchain_root}/shader-toolchain.json")
set(verifier "${CMAKE_CURRENT_LIST_DIR}/verify_shader_toolchain_manifest.cmake")
file(MAKE_DIRECTORY "${destination_absolute}")
file(LOCK "${destination_absolute}/.shader-toolchain.lock" GUARD PROCESS TIMEOUT 300)

if(EXISTS "${toolchain_root}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSTAGE=${toolchain_root} -DMANIFEST=${manifest} -P "${verifier}"
    RESULT_VARIABLE existing_result
  )
  if(NOT existing_result EQUAL 0)
    message(FATAL_ERROR "已有 Shader 工具链目录未通过完整性校验：${toolchain_root}")
  endif()
  message(STATUS "Granit Shader Toolchain 已存在并通过校验：${toolchain_root}")
  if(DEFINED RESULT_FILE AND NOT RESULT_FILE STREQUAL "")
    file(WRITE "${RESULT_FILE}" "${toolchain_root}")
  endif()
  return()
endif()

file(MAKE_DIRECTORY "${destination_absolute}/downloads")
set(archive "${destination_absolute}/downloads/${archive_name}")
set(archive_temporary "${archive}.tmp")
file(REMOVE "${archive_temporary}")
message(STATUS "正在下载 Shader 工具链：${archive_name}")
file(
  DOWNLOAD "${GRANIT_SHADER_TOOLCHAIN_RELEASE_BASE}/${archive_name}" "${archive_temporary}"
  EXPECTED_HASH "SHA256=${archive_sha256}"
  TLS_VERIFY ON
  STATUS download_status
)
list(GET download_status 0 download_code)
list(GET download_status 1 download_message)
if(NOT download_code EQUAL 0)
  message(FATAL_ERROR "下载 Shader 工具链失败：${download_message}")
endif()
file(REMOVE "${archive}")
file(RENAME "${archive_temporary}" "${archive}")

set(extract_root "${destination_absolute}/.${package_name}.extract")
cmake_path(IS_PREFIX destination_absolute "${extract_root}" NORMALIZE extract_is_scoped)
if(NOT extract_is_scoped)
  message(FATAL_ERROR "临时解包目录超出 DESTINATION")
endif()
file(REMOVE_RECURSE "${extract_root}")
file(MAKE_DIRECTORY "${extract_root}")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${extract_root}")

set(extracted_toolchain_root "${extract_root}/${package_name}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DSTAGE=${extracted_toolchain_root}
          -DMANIFEST=${extracted_toolchain_root}/shader-toolchain.json -P "${verifier}"
  RESULT_VARIABLE verify_result
)
if(NOT verify_result EQUAL 0)
  message(FATAL_ERROR "下载的 Shader 工具链未通过包内完整性校验")
endif()
file(RENAME "${extracted_toolchain_root}" "${toolchain_root}")
file(REMOVE_RECURSE "${extract_root}")

message(STATUS "Granit Shader Toolchain 已下载并通过校验：${toolchain_root}")
if(DEFINED RESULT_FILE AND NOT RESULT_FILE STREQUAL "")
  file(WRITE "${RESULT_FILE}" "${toolchain_root}")
endif()
