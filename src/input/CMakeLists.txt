# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_platform_dir "${PROJECT_SOURCE_DIR}/src/platform")

add_library(granit_input_internal OBJECT utf8.cpp)
target_compile_features(granit_input_internal PRIVATE cxx_std_20)
target_include_directories(
  granit_input_internal PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_input_internal)
set_target_properties(
  granit_input_internal
  PROPERTIES FOLDER "Modules/Internal" POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES
)
if(GRANIT_HAS_XCB)
  target_sources(granit_input_internal PRIVATE "${granit_platform_dir}/xcb/input_adapter.cpp")
endif()
if(GRANIT_HAS_WAYLAND_INPUT)
  target_sources(granit_input_internal PRIVATE "${granit_platform_dir}/wayland/input_adapter.cpp")
  target_include_directories(
    granit_input_internal SYSTEM PRIVATE "${GRANIT_XKBCOMMON_INCLUDE_DIR}"
  )
endif()

add_library(granit_input)
add_library(granit::input ALIAS granit_input)
target_sources(
  granit_input
  PRIVATE input_api.cpp
          "${granit_platform_dir}/input_adapter.cpp"
          "${granit_platform_dir}/input_adapter.h"
          $<TARGET_OBJECTS:granit_input_internal>
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/input.h"
      "${PROJECT_SOURCE_DIR}/include/granit/input.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/input/export.h"
      "${PROJECT_SOURCE_DIR}/include/granit/input/input.h"
      "${PROJECT_SOURCE_DIR}/include/granit/input/input.hpp"
)
if(GRANIT_HAS_XCB)
  target_compile_definitions(granit_input PRIVATE GRANIT_INPUT_HAS_XCB)
endif()
if(GRANIT_HAS_WAYLAND_INPUT)
  target_compile_definitions(granit_input PRIVATE GRANIT_INPUT_HAS_WAYLAND)
  target_link_libraries(granit_input PRIVATE "${GRANIT_XKBCOMMON_LIBRARY}")
endif()
if(WIN32)
  target_sources(granit_input PRIVATE "${granit_platform_dir}/win32/input_adapter.cpp")
endif()
target_compile_features(granit_input PUBLIC cxx_std_20)
target_include_directories(
  granit_input
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_compile_definitions(
  granit_input
  PRIVATE GRANIT_INPUT_BUILDING_LIBRARY
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INPUT_STATIC_DEFINE>
)
target_link_libraries(granit_input PUBLIC granit::window)
granit_target_compile_warnings(granit_input)
granit_target_output_directories(granit_input)
set_target_properties(
  granit_input
  PROPERTIES EXPORT_NAME input FOLDER "Modules" CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES
)
install(
  TARGETS granit_input
  EXPORT granitInputTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Input
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Input
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Input
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT Input
)
install(
  EXPORT granitInputTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
  COMPONENT Input
)
