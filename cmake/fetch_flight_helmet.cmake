# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR)
  get_filename_component(GRANIT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT DEFINED GRANIT_FLIGHT_HELMET_OUTPUT_DIR)
  message(FATAL_ERROR "必须通过 GRANIT_FLIGHT_HELMET_OUTPUT_DIR 指定资产输出目录")
endif()

set(manifest_path "${GRANIT_SOURCE_DIR}/examples/assets/FlightHelmet.manifest.json")
file(READ "${manifest_path}" manifest)
string(JSON schema_version GET "${manifest}" schema_version)
if(NOT schema_version EQUAL 1)
  message(FATAL_ERROR "不支持 FlightHelmet manifest schema ${schema_version}")
endif()
string(JSON base_url GET "${manifest}" base_url)
string(JSON file_count LENGTH "${manifest}" files)
math(EXPR last_file "${file_count} - 1")

foreach(index RANGE 0 ${last_file})
  string(JSON relative_path GET "${manifest}" files ${index} path)
  string(JSON sha256 GET "${manifest}" files ${index} sha256)
  set(destination "${GRANIT_FLIGHT_HELMET_OUTPUT_DIR}/${relative_path}")
  get_filename_component(destination_dir "${destination}" DIRECTORY)
  file(MAKE_DIRECTORY "${destination_dir}")
  file(
    DOWNLOAD "${base_url}/${relative_path}" "${destination}"
    EXPECTED_HASH "SHA256=${sha256}"
    STATUS download_status
    TLS_VERIFY ON
  )
  list(GET download_status 0 status_code)
  list(GET download_status 1 status_message)
  if(NOT status_code EQUAL 0)
    file(REMOVE "${destination}")
    message(FATAL_ERROR "下载 ${relative_path} 失败：${status_message}")
  endif()
endforeach()

message(STATUS "FlightHelmet 已验证到 ${GRANIT_FLIGHT_HELMET_OUTPUT_DIR}")
