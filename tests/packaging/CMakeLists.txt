# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_test(
  NAME granit.packaging.build_tree
  COMMAND
    "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_TEST_BINARY_DIR=${GRANIT_TEST_BINARY_DIR}/packaging/build-tree-consumers"
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

set_property(
  TEST
    granit.packaging.build_tree
    granit.packaging.release_version
    granit.packaging.release_version_mismatch
  APPEND PROPERTY LABELS package release
)
