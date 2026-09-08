# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
granit_prepare_sdl3_dependency(TRUE)
granit_prepare_imgui_dependency(TRUE)

add_library(
  granit_web_imgui_support STATIC
  "${granit_imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${granit_imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  "${PROJECT_SOURCE_DIR}/src/integrations/imgui/renderer.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/imgui/resources.cpp"
)
target_compile_features(granit_web_imgui_support PUBLIC cxx_std_20)
target_compile_definitions(granit_web_imgui_support PUBLIC GRANIT_INTEGRATION_IMGUI_STATIC_DEFINE)
target_include_directories(
  granit_web_imgui_support
  PUBLIC "${PROJECT_SOURCE_DIR}/examples" "${PROJECT_SOURCE_DIR}/examples/common"
         "${granit_imgui_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
  granit_web_imgui_support PUBLIC granit::render_pipeline SDL3::SDL3
                                  "${GRANIT_IMGUI_TARGET}"
)
target_compile_options(
  granit_web_imgui_support PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false"
)
granit_target_compile_warnings(granit_web_imgui_support)
set_target_properties(granit_web_imgui_support PROPERTIES FOLDER "Examples/Support")

add_executable(granit_imgui_web "${PROJECT_SOURCE_DIR}/examples/samples/imgui/web_main.cpp")
target_compile_features(granit_imgui_web PRIVATE cxx_std_20)
target_link_libraries(granit_imgui_web PRIVATE granit_web_imgui_support)
target_link_options(
  granit_imgui_web
  PRIVATE
    "--use-port=emdawnwebgpu:cpp_bindings=false"
    "--shell-file=${PROJECT_SOURCE_DIR}/examples/common/web/imgui_shell.html"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
)
set_target_properties(
  granit_imgui_web
  PROPERTIES OUTPUT_NAME granit_imgui_web SUFFIX ".html" FOLDER "Examples"
             LINK_DEPENDS "${PROJECT_SOURCE_DIR}/examples/common/web/imgui_shell.html"
)
granit_target_compile_warnings(granit_imgui_web)
set_target_properties(granit_imgui_web PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/web")
