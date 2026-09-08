# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_web_platform_smoke platform_smoke.cpp)
target_compile_features(granit_web_platform_smoke PRIVATE cxx_std_20)
target_link_libraries(granit_web_platform_smoke PRIVATE granit::granit)
target_include_directories(
  granit_web_platform_smoke PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                    "${PROJECT_SOURCE_DIR}/tests"
)
target_link_options(
  granit_web_platform_smoke
  PRIVATE
    "--use-port=emdawnwebgpu:cpp_bindings=false"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/s10d_startup.txt@/assets/s10d_startup.txt"
    "--preload-file=${PROJECT_SOURCE_DIR}/tests/assets/dynamic_uniform.vert.wgsl@/assets/dynamic_uniform.vert.wgsl"
    "--preload-file=${PROJECT_SOURCE_DIR}/tests/assets/dynamic_uniform.frag.wgsl@/assets/dynamic_uniform.frag.wgsl"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sFETCH=1"
    "-sASYNCIFY=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
)
target_link_libraries(granit_web_platform_smoke PRIVATE granit_web_model_viewer_platform)
set_target_properties(
  granit_web_platform_smoke
  PROPERTIES
    OUTPUT_NAME granit_web_platform_smoke
    SUFFIX ".html"
    FOLDER "Tests/Smoke"
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
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

set_target_properties(granit_web_platform_smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/web")

add_test(
  NAME granit.backend.native_boundaries
  COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
          -P "${PROJECT_SOURCE_DIR}/cmake/check_backend_native_boundaries.cmake"
)
