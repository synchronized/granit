# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_benchmarks
  main.cpp
  "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/retirement_queue.cpp"
)
set(granit_benchmark_revision "unknown")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE granit_benchmark_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE granit_benchmark_revision_result
    ERROR_QUIET
  )
  if(NOT granit_benchmark_revision_result EQUAL 0 OR granit_benchmark_revision STREQUAL "")
    set(granit_benchmark_revision "unknown")
  endif()
endif()
target_compile_features(granit_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_include_directories(
  granit_benchmarks
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_benchmarks)
granit_target_output_directories(granit_benchmarks)
set_target_properties(granit_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(granit_renderer_benchmarks renderer_main.cpp)
target_compile_features(granit_renderer_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_renderer_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_renderer_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_ASSET_DIR="${PROJECT_SOURCE_DIR}/examples/assets"
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_link_libraries(granit_renderer_benchmarks PRIVATE granit::granit)
granit_target_compile_warnings(granit_renderer_benchmarks)
granit_target_output_directories(granit_renderer_benchmarks)
set_target_properties(granit_renderer_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(
  granit_render_graph_benchmarks
  render_graph_main.cpp
)
target_compile_features(granit_render_graph_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_render_graph_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_render_graph_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_include_directories(granit_render_graph_benchmarks PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_render_graph_benchmarks PRIVATE granit::render_graph)
granit_target_compile_warnings(granit_render_graph_benchmarks)
granit_target_output_directories(granit_render_graph_benchmarks)
set_target_properties(granit_render_graph_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(granit_material_benchmarks material_main.cpp)
target_compile_features(granit_material_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_material_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_material_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_ASSET_DIR="${PROJECT_SOURCE_DIR}/examples/assets"
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_link_libraries(granit_material_benchmarks PRIVATE granit::material)
granit_target_compile_warnings(granit_material_benchmarks)
granit_target_output_directories(granit_material_benchmarks)
set_target_properties(granit_material_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(granit_pbr_benchmarks pbr_main.cpp)
target_compile_features(granit_pbr_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_pbr_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_pbr_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_link_libraries(granit_pbr_benchmarks PRIVATE granit::pbr)
granit_target_compile_warnings(granit_pbr_benchmarks)
granit_target_output_directories(granit_pbr_benchmarks)
set_target_properties(granit_pbr_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(granit_scene_benchmarks scene_main.cpp)
target_compile_features(granit_scene_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_scene_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_scene_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_link_libraries(granit_scene_benchmarks PRIVATE granit::scene)
granit_target_compile_warnings(granit_scene_benchmarks)
granit_target_output_directories(granit_scene_benchmarks)
set_target_properties(granit_scene_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(granit_canvas_benchmarks canvas_main.cpp)
target_compile_features(granit_canvas_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_canvas_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_canvas_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_include_directories(granit_canvas_benchmarks PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_canvas_benchmarks PRIVATE granit::render_pipeline)
granit_target_compile_warnings(granit_canvas_benchmarks)
granit_target_output_directories(granit_canvas_benchmarks)
set_target_properties(granit_canvas_benchmarks PROPERTIES FOLDER "Benchmarks")

if(GRANIT_BUILD_INTEGRATION_IMGUI)
  add_executable(granit_imgui_benchmarks imgui_main.cpp)
  target_compile_features(granit_imgui_benchmarks PRIVATE cxx_std_20)
  target_compile_options(
    granit_imgui_benchmarks
    PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
  )
  target_compile_definitions(
    granit_imgui_benchmarks
    PRIVATE
      GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
      GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
      GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
      GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
  )
  target_link_libraries(granit_imgui_benchmarks PRIVATE granit::integration_imgui)
  granit_target_compile_warnings(granit_imgui_benchmarks)
  granit_target_output_directories(granit_imgui_benchmarks)
  set_target_properties(granit_imgui_benchmarks PROPERTIES FOLDER "Benchmarks")
endif()

set(granit_canvas_benchmark_package "${CMAKE_CURRENT_BINARY_DIR}/assets/unlit_canvas.grmat")
add_custom_command(
  OUTPUT "${granit_canvas_benchmark_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND granit_material_tool build "${PROJECT_SOURCE_DIR}/examples/assets/unlit_canvas.grmat.json"
          --output "${granit_canvas_benchmark_package}"
  DEPENDS granit_material_tool "${PROJECT_SOURCE_DIR}/examples/assets/unlit_canvas.grmat.json"
  VERBATIM
)
add_executable(granit_canvas_gpu_benchmarks canvas_gpu_main.cpp "${granit_canvas_benchmark_package}")
target_compile_features(granit_canvas_gpu_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_canvas_gpu_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_canvas_gpu_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    GRANIT_CANVAS_BENCHMARK_PACKAGE="${granit_canvas_benchmark_package}"
)
target_include_directories(granit_canvas_gpu_benchmarks PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_canvas_gpu_benchmarks PRIVATE granit::render_pipeline)
granit_target_compile_warnings(granit_canvas_gpu_benchmarks)
granit_target_output_directories(granit_canvas_gpu_benchmarks)
set_target_properties(granit_canvas_gpu_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(granit_lighting_benchmarks lighting_main.cpp)
target_compile_features(granit_lighting_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_lighting_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_lighting_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
target_link_libraries(granit_lighting_benchmarks PRIVATE granit::lighting)
granit_target_compile_warnings(granit_lighting_benchmarks)
granit_target_output_directories(granit_lighting_benchmarks)
set_target_properties(granit_lighting_benchmarks PROPERTIES FOLDER "Benchmarks")

add_executable(
  granit_lighting_gpu_benchmarks
  "${PROJECT_SOURCE_DIR}/examples/pbr_offscreen.cpp"
  "${PROJECT_SOURCE_DIR}/examples/pbr_example_support.cpp"
)
target_compile_features(granit_lighting_gpu_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_lighting_gpu_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_lighting_gpu_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    GRANIT_PBR_SHADER_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
    GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
    GRANIT_PBR_GPU_BENCHMARK
)
target_link_libraries(granit_lighting_gpu_benchmarks PRIVATE granit::lighting)
granit_target_compile_warnings(granit_lighting_gpu_benchmarks)
granit_target_output_directories(granit_lighting_gpu_benchmarks)
set_target_properties(granit_lighting_gpu_benchmarks PROPERTIES FOLDER "Benchmarks")

set(granit_render_pipeline_benchmark_package
    "${CMAKE_CURRENT_BINARY_DIR}/assets/render_pipeline_workload.grmat")
add_custom_command(
  OUTPUT "${granit_render_pipeline_benchmark_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND
    granit_material_tool build
    "${PROJECT_SOURCE_DIR}/examples/assets/render_pipeline_workload.grmat.json"
    --output "${granit_render_pipeline_benchmark_package}"
  DEPENDS
    granit_material_tool
    "${PROJECT_SOURCE_DIR}/examples/assets/render_pipeline_workload.grmat.json"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights.vert.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights_untextured.frag.spv"
  VERBATIM
)
add_executable(
  granit_render_pipeline_benchmarks
  "${PROJECT_SOURCE_DIR}/examples/render_pipeline_offscreen.cpp"
)
target_compile_features(granit_render_pipeline_benchmarks PRIVATE cxx_std_20)
target_compile_options(
  granit_render_pipeline_benchmarks
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_render_pipeline_benchmarks
  PRIVATE
    GRANIT_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    GRANIT_BENCHMARK_LINK_MODE="$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    GRANIT_BENCHMARK_REVISION="${granit_benchmark_revision}"
    GRANIT_BENCHMARK_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
    GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE="${granit_render_pipeline_benchmark_package}"
)
target_link_libraries(granit_render_pipeline_benchmarks PRIVATE granit::render_pipeline)
target_include_directories(granit_render_pipeline_benchmarks PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_dependencies(granit_render_pipeline_benchmarks granit_material_tool)
target_sources(
  granit_render_pipeline_benchmarks PRIVATE "${granit_render_pipeline_benchmark_package}"
)
granit_target_compile_warnings(granit_render_pipeline_benchmarks)
granit_target_output_directories(granit_render_pipeline_benchmarks)
set_target_properties(granit_render_pipeline_benchmarks PROPERTIES FOLDER "Benchmarks")
