# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required STAGE GENERATOR DXC GLSLANG TINT DXC_VERSION GLSLANG_VERSION DAWN_VERSION
                 TINT_REVISION DXC_LICENSE_FILES GLSLANG_LICENSE_FILES DAWN_LICENSE_FILES)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

cmake_path(ABSOLUTE_PATH STAGE NORMALIZE OUTPUT_VARIABLE stage_absolute)
if(EXISTS "${stage_absolute}")
  message(FATAL_ERROR "STAGE 已存在，拒绝覆盖：${stage_absolute}")
endif()
foreach(tool DXC GLSLANG TINT GENERATOR)
  if(NOT EXISTS "${${tool}}" OR IS_DIRECTORY "${${tool}}")
    message(FATAL_ERROR "${tool} 不是有效文件：${${tool}}")
  endif()
endforeach()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef temporary_suffix)
set(working_stage "${stage_absolute}.tmp-${temporary_suffix}")
file(MAKE_DIRECTORY "${working_stage}/bin")
if(CMAKE_HOST_WIN32)
  set(executable_suffix ".exe")
else()
  set(executable_suffix "")
endif()

set(tool_files "")
foreach(tool_name dxc glslangValidator tint)
  if(tool_name STREQUAL "dxc")
    set(source "${DXC}")
  elseif(tool_name STREQUAL "glslangValidator")
    set(source "${GLSLANG}")
  else()
    set(source "${TINT}")
  endif()
  set(relative_path "bin/${tool_name}${executable_suffix}")
  file(COPY_FILE "${source}" "${working_stage}/${relative_path}")
  list(APPEND tool_files "${relative_path}")
endforeach()

foreach(runtime_file IN LISTS RUNTIME_FILES)
  if(NOT EXISTS "${runtime_file}" OR IS_DIRECTORY "${runtime_file}")
    message(FATAL_ERROR "运行库不是有效文件：${runtime_file}")
  endif()
  cmake_path(GET runtime_file FILENAME runtime_name)
  set(destination "${working_stage}/bin/${runtime_name}")
  if(EXISTS "${destination}")
    message(FATAL_ERROR "运行库目标名称冲突：${runtime_name}")
  endif()
  file(COPY_FILE "${runtime_file}" "${destination}")
endforeach()

set(license_files "")
foreach(component DXC GLSLANG DAWN)
  string(TOLOWER "${component}" component_directory)
  file(MAKE_DIRECTORY "${working_stage}/licenses/${component_directory}")
  foreach(license_file IN LISTS ${component}_LICENSE_FILES)
    if(NOT EXISTS "${license_file}" OR IS_DIRECTORY "${license_file}")
      message(FATAL_ERROR "${component} 许可证不是有效文件：${license_file}")
    endif()
    cmake_path(GET license_file FILENAME license_name)
    set(relative_path "licenses/${component_directory}/${license_name}")
    if(EXISTS "${working_stage}/${relative_path}")
      message(FATAL_ERROR "${component} 许可证目标名称冲突：${license_name}")
    endif()
    file(COPY_FILE "${license_file}" "${working_stage}/${relative_path}")
    list(APPEND license_files "${relative_path}")
  endforeach()
endforeach()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -DSTAGE=${working_stage}
    -DOUTPUT=${working_stage}/shader-toolchain.json -DDXC_VERSION=${DXC_VERSION}
    -DGLSLANG_VERSION=${GLSLANG_VERSION} -DDAWN_VERSION=${DAWN_VERSION}
    -DTINT_REVISION=${TINT_REVISION} "-DTOOL_FILES=${tool_files}"
    "-DLICENSE_FILES=${license_files}" -P "${GENERATOR}"
  RESULT_VARIABLE manifest_result
  OUTPUT_VARIABLE manifest_output
  ERROR_VARIABLE manifest_error
)
if(NOT manifest_result EQUAL 0)
  message(FATAL_ERROR "生成工具链包清单失败：${manifest_output}${manifest_error}")
endif()
file(RENAME "${working_stage}" "${stage_absolute}")
