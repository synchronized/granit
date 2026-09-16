# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(EMSCRIPTEN)
  return()
endif()

set(triangle_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(triangle_archive "${triangle_generated_dir}/triangle.grshlib")
set(triangle_index "${triangle_generated_dir}/triangle.grshidx.json")
set(triangle_cache_dir "${triangle_generated_dir}/cache")
set(triangle_ids "${triangle_generated_dir}/triangle_shader_ids.inc")

granit_add_hlsl_shader_library(
  NAME triangle
  MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/triangle.grshlib.json"
  OUTPUT "${triangle_archive}"
  INDEX "${triangle_index}"
  CACHE_DIR "${triangle_cache_dir}"
  TARGET granit_triangle_shader_library
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.hlsl"
)

add_custom_command(
  OUTPUT "${triangle_ids}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${triangle_generated_dir}"
  COMMAND "$<TARGET_FILE:granit_asset_tool>" shader index-ids
          --index "${triangle_index}"
          --shader triangle_vertex_id=triangle.vertex
          --shader triangle_fragment_id=triangle.fragment
          --output "${triangle_ids}"
  DEPENDS granit_asset_tool granit_triangle_shader_library "${triangle_index}"
  VERBATIM
)

add_executable(granit_triangle_example main.cpp "${triangle_ids}")
add_dependencies(granit_triangle_example granit_triangle_shader_library)
target_include_directories(granit_triangle_example PRIVATE "${triangle_generated_dir}")
target_compile_definitions(
  granit_triangle_example
  PRIVATE GRANIT_TRIANGLE_SHADER_LIBRARY="${triangle_archive}"
)
target_link_libraries(granit_triangle_example PRIVATE granit::granit)
target_compile_features(granit_triangle_example PRIVATE cxx_std_20)
granit_target_compile_warnings(granit_triangle_example)
granit_target_output_directories(granit_triangle_example)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(NAME granit.example.triangle COMMAND granit_triangle_example)
  set_tests_properties(
    granit.example.triangle
    PROPERTIES
      TIMEOUT 30
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
  )
  if(APPLE)
    set_tests_properties(
      granit.example.triangle
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  elseif(UNIX)
    set_tests_properties(
      granit.example.triangle
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  endif()
endif()
