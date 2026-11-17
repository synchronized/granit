# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_BINARY_DIR)
  message(FATAL_ERROR "必须指定 GRANIT_SOURCE_DIR 和 GRANIT_BINARY_DIR")
endif()

set(granit_fixture "${GRANIT_SOURCE_DIR}/tests/cmake/target_folder_fixture")
set(granit_utility "${GRANIT_SOURCE_DIR}/cmake/granit_utility.cmake")

function(granit_configure_folder_fixture name prefix)
  set(binary_dir "${GRANIT_BINARY_DIR}/${name}")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${granit_fixture}" -B "${binary_dir}"
      "-DGRANIT_UTILITY=${granit_utility}" "-DEXPECTED_PREFIX=${prefix}"
      "-DGRANIT_CMAKE_TARGET_FOLDER=${prefix}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "目标文件夹 fixture '${name}' 失败：\n${output}\n${error_output}")
  endif()
endfunction()

granit_configure_folder_fixture(default "")
granit_configure_folder_fixture(prefixed "Granit")
