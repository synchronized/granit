# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_platform_dir "${PROJECT_SOURCE_DIR}/src/platform")

add_library(granit_window)
add_library(granit::window ALIAS granit_window)
target_sources(
  granit_window
  PRIVATE window_registry.cpp
          window_backend_internal.h
          $<$<PLATFORM_ID:Windows>:${granit_platform_dir}/win32/window_backend.cpp>
          $<$<BOOL:${GRANIT_HAS_XCB}>:${granit_platform_dir}/xcb/window_backend.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${granit_platform_dir}/wayland/window_backend.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:$<TARGET_OBJECTS:granit_xdg_shell_protocol>>
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/window.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/export.h"
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
)
target_link_libraries(
  granit_window
  PRIVATE $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_LIBRARY}>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${GRANIT_WAYLAND_LIBRARY}>
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
