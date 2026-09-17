# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 编译警告只作用于 Granit 自有目标，不传递给第三方库或下游使用者。
option(GRANIT_ENABLE_WARNINGS "启用 Granit 目标编译警告" ON)
option(GRANIT_ENABLE_PEDANTIC_WARNINGS "启用 Granit 目标严格标准扩展警告" OFF)
option(GRANIT_WARNINGS_AS_ERRORS "将 Granit 目标编译警告视为错误" OFF)

# 统一 Granit 自有目标的构建产物位置，不修改父项目的全局输出目录。
function(granit_target_output_directories target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "目标不存在: ${target}")
  endif()

  if(CMAKE_CONFIGURATION_TYPES)
    set(runtime_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
    set(library_directory "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")
  else()
    set(runtime_directory "${CMAKE_BINARY_DIR}/bin")
    set(library_directory "${CMAKE_BINARY_DIR}/lib")
  endif()

  set_target_properties(
    ${target}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${runtime_directory}"
      LIBRARY_OUTPUT_DIRECTORY "${library_directory}"
      ARCHIVE_OUTPUT_DIRECTORY "${library_directory}"
  )
endfunction()

function(granit_target_compile_warnings target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "目标不存在: ${target}")
  endif()

  # clang-cl 的编译器 ID 是 Clang，但使用 MSVC 风格命令行，必须按前端变体分流。
  if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:/utf-8>)
  endif()
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # MSVC 不会为 Ninja 目标自动启用标准 C++ 异常展开；标准库容器和 Granit 的
    # 工具实现均依赖该语义，显式声明可避免 Release + /WX 产生 C4530。
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8 /EHsc>)
  endif()

  if(NOT GRANIT_ENABLE_WARNINGS)
    return()
  endif()

  if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:/W4>)
  else()
    target_compile_options(
      ${target}
      PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall>
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wextra>
    )
  endif()
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4>)
  else()
    target_compile_options(
      ${target}
      PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wconversion>
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wshorten-64-to-32>
    )
  endif()

  if(GRANIT_ENABLE_PEDANTIC_WARNINGS)
    target_compile_options(
      ${target}
      PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>
    )
  endif()

  if(NOT GRANIT_WARNINGS_AS_ERRORS)
    return()
  endif()

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    set_property(TARGET ${target} PROPERTY COMPILE_WARNING_AS_ERROR ON)
    return()
  endif()

  target_compile_options(
    ${target}
    PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,MSVC>:/WX>
      $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Werror>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Werror>
  )
endfunction()
