# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_HAS_WAYLAND_SHELL)
  add_subdirectory(platform/wayland)
endif()

add_library(granit_window)
add_library(granit::window ALIAS granit_window)
target_sources(
  granit_window
  PRIVATE system_api.cpp
          window_loop_api.cpp
          window_api.cpp
          presentation_api.cpp
          registry.cpp
          registry.h
          event_queue.cpp
          event_queue.h
          platform/backend.h
          input/input_api.cpp
          input/input_state.cpp
          input/input_state.h
          input/native_event.h
          input/platform_adapter.cpp
          input/platform_adapter.h
          input/state_utils.h
          platform/backend.cpp
          $<$<PLATFORM_ID:Windows>:platform/win32/window.cpp>
          $<$<PLATFORM_ID:Windows>:platform/win32/input.cpp>
          $<$<BOOL:${GRANIT_HAS_XCB}>:platform/xcb/window.cpp>
          $<$<BOOL:${GRANIT_HAS_XCB}>:platform/xcb/input.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:platform/wayland/window.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_INPUT}>:platform/wayland/input.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:$<TARGET_OBJECTS:granit_xdg_shell_protocol>>
          $<$<BOOL:${GRANIT_IS_EMSCRIPTEN}>:platform/emscripten/window.cpp>
          $<$<BOOL:${GRANIT_IS_EMSCRIPTEN}>:platform/emscripten/input.cpp>
          $<$<BOOL:${GRANIT_ENABLE_WINDOW_SDL3}>:platform/sdl3/window.cpp>
          $<$<BOOL:${GRANIT_ENABLE_WINDOW_SDL3}>:platform/sdl3/input.cpp>
          "${PROJECT_SOURCE_DIR}/src/core/utf8.cpp"
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/window.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/export.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/input.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/input.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/native.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/native.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/presentation.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/presentation.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window_ref.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window_loop.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window_loop.hpp"
)
target_compile_features(granit_window PUBLIC cxx_std_20)
target_include_directories(
  granit_window
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_compile_definitions(
  granit_window
  PRIVATE GRANIT_WINDOW_BUILDING_LIBRARY
          $<$<BOOL:${GRANIT_HAS_XCB}>:GRANIT_WINDOW_HAS_XCB>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:GRANIT_WINDOW_HAS_WAYLAND>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_INPUT}>:GRANIT_WINDOW_HAS_WAYLAND_INPUT>
          $<$<BOOL:${GRANIT_ENABLE_WINDOW_SDL3}>:GRANIT_WINDOW_HAS_SDL3>
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_WINDOW_STATIC_DEFINE>
)
target_include_directories(
  granit_window
  SYSTEM PRIVATE
    $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_INCLUDE_DIR}>
    $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${GRANIT_WAYLAND_INCLUDE_DIR}>
    $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${granit_wayland_protocol_dir}>
    $<$<BOOL:${GRANIT_HAS_WAYLAND_INPUT}>:${GRANIT_XKBCOMMON_INCLUDE_DIR}>
)
target_link_libraries(
  granit_window
  PUBLIC granit::granit
  PRIVATE $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_LIBRARY}>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${GRANIT_WAYLAND_LIBRARY}>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_INPUT}>:${GRANIT_XKBCOMMON_LIBRARY}>
)
if(GRANIT_ENABLE_WINDOW_SDL3)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    target_link_libraries(granit_window PRIVATE "$<BUILD_LOCAL_INTERFACE:${GRANIT_SDL3_TARGET}>")
  else()
    # CMake 3.23--3.25 没有 BUILD_LOCAL_INTERFACE；用链接产物路径避免把源码树目标
    # 写入安装导出，同时显式保留头文件传播和构建顺序。
    target_include_directories(
      granit_window SYSTEM PRIVATE
      "$<TARGET_PROPERTY:${GRANIT_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>"
    )
    target_link_libraries(
      granit_window PRIVATE "$<BUILD_INTERFACE:$<TARGET_LINKER_FILE:${GRANIT_SDL3_TARGET}>>"
    )
    add_dependencies(granit_window "${GRANIT_SDL3_TARGET}")
  endif()
  if(NOT BUILD_SHARED_LIBS)
    target_link_libraries(granit_window PRIVATE "$<INSTALL_INTERFACE:SDL3::SDL3>")
  endif()
endif()
if(GRANIT_ENABLE_WINDOW_SDL3 AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_path(GRANIT_WINDOW_X11_XCB_INCLUDE_DIR X11/Xlib-xcb.h)
  find_library(GRANIT_WINDOW_X11_XCB_LIBRARY NAMES X11-xcb)
  if(GRANIT_WINDOW_X11_XCB_INCLUDE_DIR AND GRANIT_WINDOW_X11_XCB_LIBRARY)
    target_compile_definitions(granit_window PRIVATE GRANIT_WINDOW_SDL3_HAS_X11)
    target_include_directories(
      granit_window SYSTEM PRIVATE "${GRANIT_WINDOW_X11_XCB_INCLUDE_DIR}"
    )
    target_link_libraries(granit_window PRIVATE "${GRANIT_WINDOW_X11_XCB_LIBRARY}")
  else()
    message(STATUS "Granit SDL3 Window: X11 Surface 路径禁用，未找到 X11-xcb")
  endif()
endif()
granit_target_compile_warnings(granit_window)
granit_target_output_directories(granit_window)
set_target_properties(
  granit_window
  PROPERTIES
    EXPORT_NAME window
    FOLDER "Modules"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

install(
  TARGETS granit_window
  EXPORT granitWindowTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Window
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Window
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Window
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Window
)
install(
  EXPORT granitWindowTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
  COMPONENT Window
)
