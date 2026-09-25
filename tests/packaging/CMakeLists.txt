# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_test(
  NAME granit.packaging.build_tree
  COMMAND
    "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_TEST_BINARY_DIR=${GRANIT_TEST_BINARY_DIR}/packaging/build-tree-consumers"
    "-DGRANIT_TEST_GENERATOR=${CMAKE_GENERATOR}"
    "-DGRANIT_TEST_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DGRANIT_TEST_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DGRANIT_TEST_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_build_tree_consumers.cmake"
)

add_test(
  NAME granit.packaging.release_version
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_RELEASE_TAG=v${PROJECT_VERSION}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_release_version.cmake"
)
add_test(
  NAME granit.packaging.release_version_mismatch
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -DGRANIT_RELEASE_TAG=v999.0.0
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_release_version.cmake"
)
set_tests_properties(granit.packaging.release_version_mismatch PROPERTIES WILL_FAIL TRUE)

add_test(
  NAME granit.packaging.prepare_release
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_TEST_BINARY_DIR=${GRANIT_TEST_BINARY_DIR}/packaging"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_prepare_release.cmake"
)

set_property(
  TEST
    granit.packaging.build_tree
    granit.packaging.release_version
    granit.packaging.release_version_mismatch
    granit.packaging.prepare_release
  APPEND PROPERTY LABELS package release
)
