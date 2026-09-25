# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(GRANIT_TEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(core)
add_subdirectory(window)

if(NOT GRANIT_HAS_NATIVE_WINDOW)
  add_subdirectory(web)
  return()
endif()

add_subdirectory(asset_tools)
add_subdirectory(support)
add_subdirectory(contracts/headers)
add_subdirectory(backend)
add_subdirectory(render_graph)
add_subdirectory(math)
add_subdirectory(scene)
add_subdirectory(integrations)
add_subdirectory(lighting)
add_subdirectory(window/integration)
add_subdirectory(pipeline)
add_subdirectory(material)
add_subdirectory(renderer)
add_subdirectory(contracts/abi)
add_subdirectory(packaging)

add_subdirectory(smoke)

add_test(
  NAME granit.documentation.links
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_documentation.cmake"
)

add_test(
  NAME granit.ci.shader_toolchain_contract
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_ci_workflows.cmake"
)

set_property(
  TEST granit.documentation.links granit.ci.shader_toolchain_contract
  APPEND PROPERTY LABELS unit
)
