# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_benchmarks
  main.cpp
  "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
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
