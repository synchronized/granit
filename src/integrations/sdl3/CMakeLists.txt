# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit_integration_sdl3)
add_library(granit::integration_sdl3 ALIAS granit_integration_sdl3)
target_sources(
  granit_integration_sdl3
  PRIVATE surface.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/integrations/sdl3/export.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/integrations/sdl3/surface.hpp"
)
target_compile_features(granit_integration_sdl3 PUBLIC cxx_std_20)
target_include_directories(
  granit_integration_sdl3 PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_link_libraries(granit_integration_sdl3 PUBLIC granit::granit "${GRANIT_SDL3_TARGET}")
target_compile_definitions(
  granit_integration_sdl3
  PRIVATE GRANIT_INTEGRATION_SDL3_BUILDING_LIBRARY
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INTEGRATION_SDL3_STATIC_DEFINE>
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_path(GRANIT_X11_XCB_INCLUDE_DIR X11/Xlib-xcb.h)
  find_library(GRANIT_X11_XCB_LIBRARY NAMES X11-xcb)
  if(GRANIT_X11_XCB_INCLUDE_DIR AND GRANIT_X11_XCB_LIBRARY)
    target_compile_definitions(granit_integration_sdl3 PRIVATE GRANIT_INTEGRATION_SDL3_HAS_X11)
    target_include_directories(
      granit_integration_sdl3 SYSTEM PRIVATE "${GRANIT_X11_XCB_INCLUDE_DIR}"
    )
    target_link_libraries(granit_integration_sdl3 PRIVATE "${GRANIT_X11_XCB_LIBRARY}")
  else()
    message(STATUS "Granit SDL3 Integration: X11 路径禁用，未找到 X11-xcb")
  endif()
endif()
granit_target_compile_warnings(granit_integration_sdl3)
granit_target_output_directories(granit_integration_sdl3)
set_target_properties(
  granit_integration_sdl3
  PROPERTIES
    EXPORT_NAME integration_sdl3
    FOLDER "Integrations"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
if(NOT GRANIT_SDL3_FETCHED)
  install(
    TARGETS granit_integration_sdl3
    EXPORT granitIntegrationSDL3Targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT IntegrationSDL3
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT IntegrationSDL3
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT IntegrationSDL3
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT IntegrationSDL3
  )
  install(
    EXPORT granitIntegrationSDL3Targets
    NAMESPACE granit::
    DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
    COMPONENT IntegrationSDL3
  )
endif()
