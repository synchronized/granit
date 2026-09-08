# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 检查工具链并探测原生窗口能力；保留调用目录的变量作用域。
if(GRANIT_BUILD_EMSCRIPTEN_PLATFORM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Emscripten 平台验证目标必须使用 emcmake 或 Emscripten toolchain 配置")
  endif()
  if(BUILD_SHARED_LIBS)
    message(FATAL_ERROR "Emscripten 平台验证目标只支持静态链接")
  endif()
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" --version
    RESULT_VARIABLE granit_emcc_version_result
    OUTPUT_VARIABLE granit_emcc_version_output
    ERROR_VARIABLE granit_emcc_version_error
  )
  if(NOT granit_emcc_version_result EQUAL 0 OR
     NOT granit_emcc_version_output MATCHES "[Ee]mscripten[^\n]* 5\\.0\\.6")
    message(
      FATAL_ERROR
      "S-10D 要求锁定 Emscripten 5.0.6：${granit_emcc_version_output}${granit_emcc_version_error}"
    )
  endif()
elseif(EMSCRIPTEN)
  message(FATAL_ERROR "当前 Emscripten 构建必须显式启用 GRANIT_BUILD_EMSCRIPTEN_PLATFORM")
endif()

if(NOT EMSCRIPTEN)
  set(GRANIT_HAS_XCB OFF)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GRANIT_ENABLE_XCB)
    find_path(GRANIT_XCB_INCLUDE_DIR xcb/xcb.h)
    find_library(GRANIT_XCB_LIBRARY NAMES xcb)
    if(GRANIT_XCB_INCLUDE_DIR AND GRANIT_XCB_LIBRARY)
      set(GRANIT_HAS_XCB ON)
      message(STATUS "Granit XCB Surface and Window backends enabled")
    else()
      message(STATUS "Granit XCB backends disabled: XCB development package not found")
    endif()
  endif()

  set(GRANIT_HAS_WAYLAND OFF)
  set(GRANIT_HAS_WAYLAND_SHELL OFF)
  set(GRANIT_HAS_WAYLAND_INPUT OFF)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GRANIT_ENABLE_WAYLAND)
    find_path(GRANIT_WAYLAND_INCLUDE_DIR wayland-client.h)
    find_library(GRANIT_WAYLAND_LIBRARY NAMES wayland-client)
    if(GRANIT_WAYLAND_INCLUDE_DIR AND GRANIT_WAYLAND_LIBRARY)
      set(GRANIT_HAS_WAYLAND ON)
      message(STATUS "Granit Wayland Surface backend enabled")
      find_program(GRANIT_WAYLAND_SCANNER wayland-scanner)
      find_file(
        GRANIT_XDG_SHELL_PROTOCOL
        xdg-shell.xml
        PATHS /usr/share /usr/local/share
        PATH_SUFFIXES wayland-protocols/stable/xdg-shell
      )
      if(GRANIT_WAYLAND_SCANNER AND GRANIT_XDG_SHELL_PROTOCOL)
        set(GRANIT_HAS_WAYLAND_SHELL ON)
        message(STATUS "Granit Wayland xdg-shell Window backend enabled")
        find_path(GRANIT_XKBCOMMON_INCLUDE_DIR xkbcommon/xkbcommon.h)
        find_library(GRANIT_XKBCOMMON_LIBRARY NAMES xkbcommon)
        if(GRANIT_XKBCOMMON_INCLUDE_DIR AND GRANIT_XKBCOMMON_LIBRARY)
          set(GRANIT_HAS_WAYLAND_INPUT ON)
          message(STATUS "Granit Wayland Input backend enabled")
        else()
          message(STATUS "Granit Wayland Input backend disabled: xkbcommon not found")
        endif()
      else()
        message(STATUS "Granit Wayland Window backend disabled: protocol tools not found")
      endif()
    else()
      message(STATUS
        "Granit Wayland Surface backend disabled: Wayland development package not found"
      )
    endif()
  endif()

  set(granit_wayland_protocol_dir "${PROJECT_BINARY_DIR}/generated/wayland")
endif()
