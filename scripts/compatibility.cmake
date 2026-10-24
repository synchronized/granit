# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors
cmake_minimum_required(VERSION 3.23)
if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_MATRIX_ROW)
  message(FATAL_ERROR "需要源码目录和矩阵行 JSON")
endif()
foreach(key IN ITEMS id cc cxx expected linkage config sanitizer)
  string(JSON ${key} GET "${GRANIT_MATRIX_ROW}" ${key})
endforeach()
if(NOT id MATCHES "^[a-z0-9-]+$")
  message(FATAL_ERROR "非法矩阵标识")
endif()
set(build "${GRANIT_SOURCE_DIR}/build/compatibility/${id}")
set(report "${build}/report")
file(MAKE_DIRECTORY "${report}/logs")
set(ENV{CC} "${cc}")
set(ENV{CXX} "${cxx}")
set(ENV{CMAKE_GENERATOR} Ninja)
file(WRITE "${report}/requested-environment.json" "${GRANIT_MATRIX_ROW}\n")
if(CMAKE_HOST_WIN32)
  execute_process(COMMAND powershell -NoProfile -Command
    "Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion | ConvertTo-Json"
    OUTPUT_FILE "${report}/gpu-driver.json")
else()
  execute_process(COMMAND vulkaninfo --summary OUTPUT_FILE "${report}/gpu-driver.txt"
    ERROR_FILE "${report}/logs/vulkaninfo.log" TIMEOUT 20)
endif()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${GRANIT_SOURCE_DIR}"
  OUTPUT_FILE "${report}/commit.txt" COMMAND_ERROR_IS_FATAL ANY)
function(run stage)
  file(WRITE "${report}/logs/${stage}-command.txt" "${ARGN}\n")
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${GRANIT_SOURCE_DIR}"
    RESULT_VARIABLE result OUTPUT_FILE "${report}/logs/${stage}.log"
    ERROR_FILE "${report}/logs/${stage}.log")
  if(NOT result STREQUAL "0")
    file(WRITE "${report}/summary.json"
      "{\"status\":\"failed\",\"stage\":\"${stage}\",\"exit_code\":\"${result}\"}\n")
    file(READ "${report}/logs/${stage}.log" output)
    message("${output}")
    message(FATAL_ERROR "${id} ${stage} 失败：${result}")
  endif()
endfunction()
set(toolchain_args)
if(DEFINED GRANIT_SHADER_TOOLCHAIN_ROOT)
  include("${GRANIT_SOURCE_DIR}/cmake/toolchain/GranitShaderToolchainLock.cmake")
  list(APPEND toolchain_args "-DGRANIT_SHADER_TOOLCHAIN_ROOT=${GRANIT_SHADER_TOOLCHAIN_ROOT}"
    -DGRANIT_SHADER_TOOLCHAIN_POLICY=locked
    "-DGRANIT_TINT_REVISION=${GRANIT_SHADER_TOOLCHAIN_TINT_REVISION}")
endif()
if(CMAKE_HOST_WIN32 AND DEFINED ENV{VCToolsInstallDir})
  list(APPEND toolchain_args "-DCMAKE_LINKER=$ENV{VCToolsInstallDir}/bin/Hostx64/x64/link.exe")
endif()
run(configure "${CMAKE_COMMAND}" -S "${GRANIT_SOURCE_DIR}" -B "${build}" -G Ninja
  "-DCMAKE_C_COMPILER=${cc}" "-DCMAKE_CXX_COMPILER=${cxx}"
  "-DCMAKE_BUILD_TYPE=${config}" "-DBUILD_SHARED_LIBS=${linkage}"
  "-DGRANIT_SANITIZER=${sanitizer}" -DGRANIT_BUILD_EXAMPLES=OFF
  -DGRANIT_ENABLE_WINDOW_SDL3=OFF -DGRANIT_BUILD_ASSET_TOOLS=ON
  -DGRANIT_WARNINGS_AS_ERRORS=ON -DGRANIT_VERIFY_SHADER_ASSET_SNAPSHOTS=OFF ${toolchain_args})
file(READ "${build}/validation-environment.json" environment)
file(COPY "${build}/validation-environment.json" DESTINATION "${report}")
foreach(language IN ITEMS C CXX)
  string(JSON actual GET "${environment}" CMAKE_${language}_COMPILER_VERSION)
  string(FIND "${actual}" "${expected}" position)
  if(NOT position EQUAL 0)
    file(WRITE "${report}/summary.json" "{\"status\":\"failed\",\"stage\":\"compiler-identity\"}\n")
    message(FATAL_ERROR "${language} 实际版本 ${actual} 不匹配 ${expected}")
  endif()
endforeach()
if(sanitizer STREQUAL "thread")
  run(build "${CMAKE_COMMAND}" --build "${build}" --parallel 2 --target
    granit_handle_table_test granit_async_operation_state_test granit_retirement_queue_test)
else()
  run(build "${CMAKE_COMMAND}" --build "${build}" --parallel 2)
endif()
set(selection -LE "^stress$")
if(CMAKE_HOST_WIN32)
  set(selection -LE "^(gpu|platform|stress)$" -E "^granit\\.example\\.")
elseif(sanitizer STREQUAL "thread")
  # TSan 限定已有 CPU 并发用例；不能将不执行的 GPU 测试写为通过。
  set(selection -R "^granit\\.core\\.(handle_table|async_operation_state|retirement_queue)$")
endif()
run(test "${CMAKE_CTEST_COMMAND}" --test-dir "${build}" --output-on-failure
  --no-tests=error --output-junit "${report}/junit.xml" ${selection})
if(sanitizer STREQUAL "none")
  set(prefix "${build}/install")
  run(install "${CMAKE_COMMAND}" --install "${build}" --prefix "${prefix}")
  run(exports "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${GRANIT_SOURCE_DIR}"
    "-DGRANIT_BUILD_DIR=${build}" "-DGRANIT_INSTALL_PREFIX=${prefix}"
    -P "${GRANIT_SOURCE_DIR}/tests/cmake/check_install_exports.cmake")
  run(package "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${GRANIT_SOURCE_DIR}"
    "-DGRANIT_INSTALL_PREFIX=${prefix}" "-DGRANIT_TEST_BINARY_DIR=${build}/package"
    "-DGRANIT_TEST_SHADER_TOOLCHAIN_ROOT=${GRANIT_SHADER_TOOLCHAIN_ROOT}"
    "-DGRANIT_TEST_CONFIGURATION=${config}"
    -P "${GRANIT_SOURCE_DIR}/tests/packaging/check_installed_package.cmake")
  run(consumer-configure "${CMAKE_COMMAND}" -S "${GRANIT_SOURCE_DIR}/tests/packaging/consumer"
    -B "${build}/consumer" -G Ninja "-DCMAKE_BUILD_TYPE=${config}"
    "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_C_COMPILER=${cc}" "-DCMAKE_CXX_COMPILER=${cxx}")
  run(consumer-build "${CMAKE_COMMAND}" --build "${build}/consumer")
  run(consumer-test "${CMAKE_CTEST_COMMAND}" --test-dir "${build}/consumer"
    --output-on-failure --no-tests=error --output-junit "${report}/consumer-junit.xml")
endif()
file(READ "${report}/junit.xml" junit)
set(status passed)
if(junit MATCHES "<skipped" OR CMAKE_HOST_WIN32)
  set(status incomplete)
endif()
file(WRITE "${report}/summary.json" "{\"status\":\"${status}\",\"stage\":\"complete\"}\n")
message(STATUS "${id}：${status}，报告 ${report}")
