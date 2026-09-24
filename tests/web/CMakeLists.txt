# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS browser integration)

include(granit_web)

add_executable(granit_web_platform_smoke platform_smoke.cpp sdl3_window_smoke.cpp)
target_compile_features(granit_web_platform_smoke PRIVATE cxx_std_20)
target_link_libraries(granit_web_platform_smoke PRIVATE granit::granit)
target_include_directories(
  granit_web_platform_smoke PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                    "${PROJECT_SOURCE_DIR}/tests"
)
target_link_options(
  granit_web_platform_smoke
  PRIVATE
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/s10d_startup.txt@/assets/s10d_startup.txt"
    "--preload-file=${granit_test_shader_fixture_dir}/dynamic_uniform.vert.wgsl@/assets/dynamic_uniform.vert.wgsl"
    "--preload-file=${granit_test_shader_fixture_dir}/dynamic_uniform.frag.wgsl@/assets/dynamic_uniform.frag.wgsl"
)
target_link_libraries(granit_web_platform_smoke PRIVATE granit_web_model_viewer_platform)
set_target_properties(
  granit_web_platform_smoke
  PROPERTIES
    OUTPUT_NAME granit_web_platform_smoke

    FOLDER "Tests/Smoke"
)
granit_target_compile_warnings(granit_web_platform_smoke)

add_custom_command(
  TARGET granit_web_platform_smoke
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/model_viewer_fixture.gltf"
    "$<TARGET_FILE_DIR:granit_web_platform_smoke>/model_viewer_fixture.gltf"
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/model_viewer_fixture.bin"
    "$<TARGET_FILE_DIR:granit_web_platform_smoke>/model_viewer_fixture.bin"
  VERBATIM
)

add_test(
  NAME granit.backend.native_boundaries
  COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
          -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_backend_native_boundaries.cmake"
)

granit_target_web_page(granit_web_platform_smoke "${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
