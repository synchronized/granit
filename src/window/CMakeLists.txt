# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_HAS_WAYLAND_SHELL)
  add_subdirectory(platform/wayland)
endif()

add_library(granit_window_input_internal OBJECT "${PROJECT_SOURCE_DIR}/src/core/utf8.cpp")
target_compile_features(granit_window_input_internal PRIVATE cxx_std_20)
target_include_directories(
  granit_window_input_internal PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
if(GRANIT_HAS_XCB)
  target_sources(granit_window_input_internal PRIVATE platform/xcb/input.cpp)
endif()
if(GRANIT_HAS_WAYLAND_INPUT)
  target_sources(granit_window_input_internal PRIVATE platform/wayland/input.cpp)
  target_include_directories(
    granit_window_input_internal SYSTEM PRIVATE "${GRANIT_XKBCOMMON_INCLUDE_DIR}"
  )
endif()
if(GRANIT_IS_EMSCRIPTEN)
  target_sources(granit_window_input_internal PRIVATE platform/emscripten/input.cpp)
endif()
granit_target_compile_warnings(granit_window_input_internal)
set_target_properties(
  granit_window_input_internal
  PROPERTIES FOLDER "Modules/Internal" POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES
)

add_library(granit_window)
add_library(granit::window ALIAS granit_window)
target_sources(
  granit_window
  PRIVATE system_api.cpp
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
          $<TARGET_OBJECTS:granit_window_input_internal>
          $<$<PLATFORM_ID:Windows>:platform/win32/window.cpp>
          $<$<PLATFORM_ID:Windows>:platform/win32/input.cpp>
          $<$<BOOL:${GRANIT_HAS_XCB}>:platform/xcb/window.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:platform/wayland/window.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:$<TARGET_OBJECTS:granit_xdg_shell_protocol>>
          $<$<BOOL:${GRANIT_IS_EMSCRIPTEN}>:platform/emscripten/window.cpp>
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
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_WINDOW_STATIC_DEFINE>
)
target_include_directories(
  granit_window SYSTEM PRIVATE $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_INCLUDE_DIR}>
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
