# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include(granit_web)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

add_library(
  granit_web_imgui_support STATIC
  "${PROJECT_SOURCE_DIR}/examples/samples/imgui/resources.cpp"
)
target_compile_features(granit_web_imgui_support PUBLIC cxx_std_20)
target_include_directories(
  granit_web_imgui_support
  PUBLIC "${PROJECT_SOURCE_DIR}/examples" "${PROJECT_SOURCE_DIR}/examples/common"
         "${granit_imgui_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
  granit_web_imgui_support PUBLIC granit::integration_sdl3 granit::integration_imgui
)
target_link_libraries(
  granit_web_imgui_support PUBLIC ${GRANIT_IMGUI_DEMO_TARGET} ${GRANIT_IMGUI_BACKEND_SDL3_TARGET}
)
granit_target_webgpu(granit_web_imgui_support)
granit_target_compile_warnings(granit_web_imgui_support)
set_target_properties(granit_web_imgui_support PROPERTIES FOLDER "Examples/Support")

add_executable(granit_imgui_web "${PROJECT_SOURCE_DIR}/examples/samples/imgui/web_main.cpp")
target_compile_features(granit_imgui_web PRIVATE cxx_std_20)
target_link_libraries(granit_imgui_web PRIVATE granit_web_imgui_support)

set_target_properties(
  granit_imgui_web
  PROPERTIES OUTPUT_NAME granit_imgui_web FOLDER "Examples"
)
granit_target_compile_warnings(granit_imgui_web)

granit_target_web_page(
  granit_imgui_web "${CMAKE_CURRENT_LIST_DIR}/imgui_shell.html"
)
