# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required PACKAGER GENERATOR VERIFIER WORK_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/inputs")
foreach(name dxc glslang tint runtime dxc-license glslang-license dawn-license)
  file(WRITE "${WORK_DIR}/inputs/${name}" "${name} fixture")
endforeach()
set(stage "${WORK_DIR}/package")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -DSTAGE=${stage} -DGENERATOR=${GENERATOR}
    -DDXC=${WORK_DIR}/inputs/dxc -DGLSLANG=${WORK_DIR}/inputs/glslang
    -DTINT=${WORK_DIR}/inputs/tint -DDXC_VERSION=fixture -DGLSLANG_VERSION=fixture
    -DDAWN_VERSION=fixture -DTINT_REVISION=fixture
    -DDXC_LICENSE_FILES=${WORK_DIR}/inputs/dxc-license
    -DGLSLANG_LICENSE_FILES=${WORK_DIR}/inputs/glslang-license
    -DDAWN_LICENSE_FILES=${WORK_DIR}/inputs/dawn-license
    -DRUNTIME_FILES=${WORK_DIR}/inputs/runtime -P "${PACKAGER}"
  RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output
  ERROR_VARIABLE package_error
)
if(NOT package_result EQUAL 0)
  message(FATAL_ERROR "组装工具链包失败：${package_output}${package_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DSTAGE=${stage}
          -DMANIFEST=${stage}/shader-toolchain.json -P "${VERIFIER}"
  RESULT_VARIABLE verify_result
  OUTPUT_VARIABLE verify_output
  ERROR_VARIABLE verify_error
)
if(NOT verify_result EQUAL 0)
  message(FATAL_ERROR "组装后的工具链包验证失败：${verify_output}${verify_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DSTAGE=${stage} -DGENERATOR=${GENERATOR}
          -DDXC=${WORK_DIR}/inputs/dxc -DGLSLANG=${WORK_DIR}/inputs/glslang
          -DTINT=${WORK_DIR}/inputs/tint -DDXC_VERSION=fixture
          -DGLSLANG_VERSION=fixture -DDAWN_VERSION=fixture -DTINT_REVISION=fixture
          -DDXC_LICENSE_FILES=${WORK_DIR}/inputs/dxc-license
          -DGLSLANG_LICENSE_FILES=${WORK_DIR}/inputs/glslang-license
          -DDAWN_LICENSE_FILES=${WORK_DIR}/inputs/dawn-license -P "${PACKAGER}"
  RESULT_VARIABLE overwrite_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(overwrite_result EQUAL 0)
  message(FATAL_ERROR "组包脚本意外覆盖已有目录")
endif()

