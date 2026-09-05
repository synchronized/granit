# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED DESTINATION OR DESTINATION STREQUAL "")
  message(FATAL_ERROR "必须通过 DESTINATION 指定工具链缓存目录")
endif()

set(toolchain_version "v20260720.160313")
set(tint_revision "0bc38adde72b79013536f8ce354b639ae19ae195")
set(release_tag "shader-toolchain-${toolchain_version}-0bc38adde72b")
set(release_base "https://github.com/synchronized/granit/releases/download/${release_tag}")

if(CMAKE_HOST_WIN32)
  set(package_name "granit-shader-toolchain-${toolchain_version}-windows-x64")
  set(archive_name "${package_name}.zip")
  set(archive_sha256 "4726f9dc631c25b5aec18db3b2a0c416b6d9b45b5050f2c6c71b66c28ea98626")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(package_name "granit-shader-toolchain-${toolchain_version}-linux-x64")
  set(archive_name "${package_name}.tar.gz")
  set(archive_sha256 "a0ad5b884f66ad924d4594289f11bf39dc661bde579b6bc8e85238c414de219c")
else()
  message(FATAL_ERROR "当前仅提供 Windows x64 与 Linux x64 Shader 工具链包")
endif()

cmake_path(ABSOLUTE_PATH DESTINATION NORMALIZE OUTPUT_VARIABLE destination_absolute)
set(toolchain_root "${destination_absolute}/${package_name}")
set(manifest "${toolchain_root}/shader-toolchain.json")
set(verifier "${CMAKE_CURRENT_LIST_DIR}/verify_shader_toolchain_manifest.cmake")

if(EXISTS "${toolchain_root}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSTAGE=${toolchain_root} -DMANIFEST=${manifest} -P "${verifier}"
    RESULT_VARIABLE existing_result
  )
  if(NOT existing_result EQUAL 0)
    message(FATAL_ERROR "已有 Shader 工具链目录未通过完整性校验：${toolchain_root}")
  endif()
  message(STATUS "Granit Shader Toolchain 已存在并通过校验：${toolchain_root}")
  message(STATUS "配置时设置 GRANIT_TINT_REVISION=${tint_revision}")
  return()
endif()

file(MAKE_DIRECTORY "${destination_absolute}/downloads")
set(archive "${destination_absolute}/downloads/${archive_name}")
file(
  DOWNLOAD "${release_base}/${archive_name}" "${archive}"
  EXPECTED_HASH "SHA256=${archive_sha256}"
  TLS_VERIFY ON
  STATUS download_status
  SHOW_PROGRESS
)
list(GET download_status 0 download_code)
list(GET download_status 1 download_message)
if(NOT download_code EQUAL 0)
  message(FATAL_ERROR "下载 Shader 工具链失败：${download_message}")
endif()

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
message(STATUS "配置时设置 GRANIT_TINT_REVISION=${tint_revision}")
