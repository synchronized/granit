# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required STAGE OUTPUT COMPONENT PLATFORM ARCH LOADER_VERSION LOADER_FILES LICENSE_FILES)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

cmake_path(ABSOLUTE_PATH STAGE NORMALIZE OUTPUT_VARIABLE stage_absolute)
cmake_path(ABSOLUTE_PATH OUTPUT NORMALIZE OUTPUT_VARIABLE output_absolute)
cmake_path(IS_PREFIX stage_absolute "${output_absolute}" NORMALIZE output_is_in_stage)
if(NOT IS_DIRECTORY "${stage_absolute}" OR NOT output_is_in_stage)
  message(FATAL_ERROR "STAGE 必须存在，OUTPUT 必须位于 STAGE 内")
endif()

function(granit_json_escape input output)
  string(REPLACE "\\" "\\\\" escaped "${input}")
  string(REPLACE "\"" "\\\"" escaped "${escaped}")
  string(REPLACE "\n" "\\n" escaped "${escaped}")
  set(${output} "${escaped}" PARENT_SCOPE)
endfunction()

set(required_paths ${LOADER_FILES} ${LICENSE_FILES})
if(DEFINED VALIDATION_FILES)
  list(APPEND required_paths ${VALIDATION_FILES})
endif()
foreach(relative_path IN LISTS required_paths)
  cmake_path(ABSOLUTE_PATH relative_path BASE_DIRECTORY "${stage_absolute}" NORMALIZE
             OUTPUT_VARIABLE absolute_path)
  cmake_path(IS_PREFIX stage_absolute "${absolute_path}" NORMALIZE path_is_in_stage)
  if(NOT path_is_in_stage OR NOT EXISTS "${absolute_path}" OR IS_DIRECTORY "${absolute_path}")
    message(FATAL_ERROR "Vulkan Runtime 包缺少必需文件：${relative_path}")
  endif()
endforeach()

file(GLOB_RECURSE package_files LIST_DIRECTORIES false RELATIVE "${stage_absolute}"
     "${stage_absolute}/*")
cmake_path(RELATIVE_PATH output_absolute BASE_DIRECTORY "${stage_absolute}"
           OUTPUT_VARIABLE output_relative)
list(REMOVE_ITEM package_files "${output_relative}")
list(SORT package_files)

set(file_records "")
foreach(relative_path IN LISTS package_files)
  set(absolute_path "${stage_absolute}/${relative_path}")
  file(SHA256 "${absolute_path}" sha256)
  file(SIZE "${absolute_path}" byte_size)
  list(FIND LOADER_FILES "${relative_path}" loader_index)
  list(FIND LICENSE_FILES "${relative_path}" license_index)
  if(DEFINED VALIDATION_FILES)
    list(FIND VALIDATION_FILES "${relative_path}" validation_index)
  else()
    set(validation_index -1)
  endif()
  if(NOT loader_index EQUAL -1)
    set(role loader)
  elseif(NOT validation_index EQUAL -1)
    set(role validation_layer)
  elseif(NOT license_index EQUAL -1)
    set(role license)
  else()
    set(role runtime)
  endif()
  granit_json_escape("${relative_path}" escaped_path)
  if(file_records)
    string(APPEND file_records ",\n")
  endif()
  string(APPEND file_records
         "    {\"path\":\"${escaped_path}\",\"size\":${byte_size},"
         "\"sha256\":\"${sha256}\",\"role\":\"${role}\"}")
endforeach()

foreach(metadata COMPONENT PLATFORM ARCH LOADER_VERSION)
  list(LENGTH ${metadata} metadata_length)
  if(NOT metadata_length EQUAL 1)
    message(FATAL_ERROR "${metadata} 必须是单个不含分号的标识")
  endif()
  granit_json_escape("${${metadata}}" escaped_${metadata})
endforeach()
if(NOT DEFINED VALIDATION_VERSION)
  set(VALIDATION_VERSION "")
endif()
granit_json_escape("${VALIDATION_VERSION}" escaped_VALIDATION_VERSION)

string(CONCAT manifest
    "{\n"
    "  \"schema\":1,\n"
    "  \"component\":\"${escaped_COMPONENT}\",\n"
    "  \"platform\":\"${escaped_PLATFORM}\",\n"
    "  \"arch\":\"${escaped_ARCH}\",\n"
    "  \"loader_version\":\"${escaped_LOADER_VERSION}\",\n"
    "  \"validation_version\":\"${escaped_VALIDATION_VERSION}\",\n"
    "  \"files\":[\n${file_records}\n  ]\n"
    "}\n")
set(temporary "${output_absolute}.tmp")
file(WRITE "${temporary}" "${manifest}")
file(RENAME "${temporary}" "${output_absolute}")
