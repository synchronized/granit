# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(GRANIT_SANITIZER "none" CACHE STRING "自有目标插桩：none、address-undefined 或 thread")
set_property(CACHE GRANIT_SANITIZER PROPERTY STRINGS none address-undefined thread)
if(NOT GRANIT_SANITIZER MATCHES "^(none|address-undefined|thread)$")
  message(FATAL_ERROR "未知 Sanitizer：${GRANIT_SANITIZER}")
endif()
if(NOT GRANIT_SANITIZER STREQUAL "none" AND
   (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  message(FATAL_ERROR "Sanitizer 验收当前仅支持 Linux Clang")
endif()

function(granit_target_sanitizer target)
  if(GRANIT_SANITIZER STREQUAL "none")
    return()
  endif()
  if(GRANIT_SANITIZER STREQUAL "thread")
    set(kind thread)
  else()
    set(kind address,undefined)
  endif()
  # PRIVATE 保证安装目标和 Consumer 不继承测试插桩设置。
  target_compile_options(${target} PRIVATE "-fsanitize=${kind}" -fno-omit-frame-pointer
                         -fno-sanitize-recover=all)
  get_target_property(type ${target} TYPE)
  if(NOT type STREQUAL "OBJECT_LIBRARY" AND NOT type STREQUAL "STATIC_LIBRARY")
    target_link_options(${target} PRIVATE "-fsanitize=${kind}")
  endif()
endfunction()

# 配置时记录实际工具链，不把 Runner 名称当作编译器身份。
set(granit_environment "{}")
foreach(key IN ITEMS CMAKE_C_COMPILER CMAKE_C_COMPILER_ID CMAKE_C_COMPILER_VERSION
                     CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ID CMAKE_CXX_COMPILER_VERSION
                     CMAKE_SYSTEM_NAME CMAKE_SYSTEM_PROCESSOR CMAKE_BUILD_TYPE
                     CMAKE_GENERATOR CMAKE_VERSION BUILD_SHARED_LIBS GRANIT_PROJECT_VERSION
                     GRANIT_SANITIZER GRANIT_ENABLE_STRESS_TESTS GRANIT_STRESS_SECONDS
                     GRANIT_STRESS_SEED GRANIT_STRESS_ITERATIONS)
  set(value "${${key}}")
  string(REPLACE "\\" "/" value "${value}")
  string(REPLACE "\"" "\\\"" value "${value}")
  string(JSON granit_environment SET "${granit_environment}" "${key}" "\"${value}\"")
endforeach()
foreach(language IN ITEMS C CXX)
  if(EXISTS "${CMAKE_${language}_COMPILER}")
    file(SHA256 "${CMAKE_${language}_COMPILER}" compiler_digest)
    string(JSON granit_environment SET "${granit_environment}" "${language}_COMPILER_SHA256"
      "\"${compiler_digest}\"")
  endif()
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/validation-environment.json" "${granit_environment}\n")

function(granit_register_stress name target)
  if(NOT GRANIT_STRESS_SECONDS MATCHES "^[0-9]+$" OR
     GRANIT_STRESS_SECONDS LESS 1 OR GRANIT_STRESS_SECONDS GREATER 86400 OR
     NOT GRANIT_STRESS_SEED MATCHES "^[0-9]+$" OR GRANIT_STRESS_SEED GREATER 4294967295 OR
     NOT GRANIT_STRESS_ITERATIONS MATCHES "^[0-9]+$" OR
     GRANIT_STRESS_ITERATIONS GREATER 4294967295)
    message(FATAL_ERROR "长测需要 1..86400 秒，以及 32 位无符号种子和迭代上限")
  endif()
  math(EXPR timeout "${GRANIT_STRESS_SECONDS} + 120")
  set(environment "GRANIT_STRESS_SECONDS=${GRANIT_STRESS_SECONDS}"
    "GRANIT_STRESS_SEED=${GRANIT_STRESS_SEED}" "GRANIT_VULKAN_VALIDATION=on"
    "GRANIT_STRESS_ITERATIONS=${GRANIT_STRESS_ITERATIONS}" "GRANIT_FRAME_TRACE_MAX_EVENTS=512")
  add_test(NAME ${name} COMMAND ${target} ${ARGN})
  set_tests_properties(${name} PROPERTIES LABELS "stress;gpu;integration"
    TIMEOUT ${timeout} RESOURCE_LOCK granit_gpu
    ENVIRONMENT "${environment}"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
    FAIL_REGULAR_EXPRESSION "\\[granit\\]\\[lifecycle\\]")
endfunction()

function(granit_stress_target target source)
  add_executable(${target} "${source}")
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_link_libraries(${target} PRIVATE granit::granit granit_test_catch2_with_main)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  if(WIN32)
    target_link_libraries(${target} PRIVATE Psapi)
  endif()
  granit_target_compile_warnings(${target})
  granit_target_output_directories(${target})
endfunction()
