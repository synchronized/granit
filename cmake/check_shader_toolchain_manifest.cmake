# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required GENERATOR VERIFIER WORK_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/bin" "${WORK_DIR}/licenses")
file(WRITE "${WORK_DIR}/bin/dxc" "dxc fixture")
file(WRITE "${WORK_DIR}/bin/glslangValidator" "glslang fixture")
file(WRITE "${WORK_DIR}/bin/tint" "tint fixture")
file(WRITE "${WORK_DIR}/licenses/THIRD_PARTY.txt" "license fixture")
set(manifest "${WORK_DIR}/shader-toolchain.json")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -DSTAGE=${WORK_DIR} -DOUTPUT=${manifest} -DDXC_VERSION=fixture
    -DGLSLANG_VERSION=fixture -DDAWN_VERSION=fixture -DTINT_REVISION=fixture
    "-DTOOL_FILES=bin/dxc;bin/glslangValidator;bin/tint"
    -DLICENSE_FILES=licenses/THIRD_PARTY.txt -P "${GENERATOR}"
  RESULT_VARIABLE generate_result
  OUTPUT_VARIABLE generate_output
  ERROR_VARIABLE generate_error
)
if(NOT generate_result EQUAL 0)
  message(FATAL_ERROR "生成工具包清单失败：${generate_output}${generate_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DSTAGE=${WORK_DIR} -DMANIFEST=${manifest} -P "${VERIFIER}"
  RESULT_VARIABLE verify_result
  OUTPUT_VARIABLE verify_output
  ERROR_VARIABLE verify_error
)
if(NOT verify_result EQUAL 0)
  message(FATAL_ERROR "验证工具包清单失败：${verify_output}${verify_error}")
endif()

file(APPEND "${WORK_DIR}/bin/tint" "tampered")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DSTAGE=${WORK_DIR} -DMANIFEST=${manifest} -P "${VERIFIER}"
  RESULT_VARIABLE tamper_result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(tamper_result EQUAL 0)
  message(FATAL_ERROR "被篡改的工具包意外通过验证")
endif()
