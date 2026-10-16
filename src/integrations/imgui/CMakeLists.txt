# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT TARGET ${GRANIT_IMGUI_TARGET})
  return()
endif()

add_library(granit_integration_imgui)
add_library(granit::integration_imgui ALIAS granit_integration_imgui)
target_sources(
  granit_integration_imgui
  PRIVATE renderer.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/integrations/imgui/export.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/integrations/imgui/input.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/integrations/imgui/renderer.hpp"
)
target_compile_features(granit_integration_imgui PUBLIC cxx_std_20)
target_include_directories(
  granit_integration_imgui PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
get_target_property(granit_imgui_include_dirs "${GRANIT_IMGUI_TARGET}" INTERFACE_INCLUDE_DIRECTORIES)
if(granit_imgui_include_dirs)
  target_include_directories(granit_integration_imgui PRIVATE ${granit_imgui_include_dirs})
endif()
target_link_libraries(
  granit_integration_imgui PUBLIC granit::render_pipeline
)
target_link_libraries(granit_integration_imgui INTERFACE "${GRANIT_IMGUI_TARGET}")
target_compile_definitions(
  granit_integration_imgui
  PRIVATE GRANIT_INTEGRATION_IMGUI_BUILDING_LIBRARY
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INTEGRATION_IMGUI_STATIC_DEFINE>
)
granit_target_compile_warnings(granit_integration_imgui)
granit_target_output_directories(granit_integration_imgui)
set_target_properties(
  granit_integration_imgui
  PROPERTIES
    EXPORT_NAME integration_imgui
    FOLDER "Integrations"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
if(NOT GRANIT_IMGUI_FETCHED)
  install(
    TARGETS granit_integration_imgui
    EXPORT granitIntegrationImGuiTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT IntegrationImGui
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT IntegrationImGui
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT IntegrationImGui
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT IntegrationImGui
  )
  install(
    EXPORT granitIntegrationImGuiTargets
    NAMESPACE granit::
    DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
    COMPONENT IntegrationImGui
  )
endif()

add_library(granit_integration_imgui_window INTERFACE)
add_library(granit::integration_imgui_window ALIAS granit_integration_imgui_window)
target_sources(
  granit_integration_imgui_window
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES "${PROJECT_SOURCE_DIR}/include/granit/integrations/imgui/window_input.hpp"
)
target_compile_features(granit_integration_imgui_window INTERFACE cxx_std_20)
target_include_directories(
  granit_integration_imgui_window INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_link_libraries(
  granit_integration_imgui_window INTERFACE granit::integration_imgui granit::window
)
target_compile_definitions(
  granit_integration_imgui_window
  INTERFACE $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INTEGRATION_IMGUI_STATIC_DEFINE>
)
set_target_properties(
  granit_integration_imgui_window
  PROPERTIES
    EXPORT_NAME integration_imgui_window
    FOLDER "Integrations"
)
if(NOT GRANIT_IMGUI_FETCHED)
  install(
    TARGETS granit_integration_imgui_window
    EXPORT granitIntegrationImGuiWindowTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT IntegrationImGuiWindow
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT IntegrationImGuiWindow
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT IntegrationImGuiWindow
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT IntegrationImGuiWindow
  )
  install(
    EXPORT granitIntegrationImGuiWindowTargets
    NAMESPACE granit::
    DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
    COMPONENT IntegrationImGuiWindow
  )
endif()
