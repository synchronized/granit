# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_stable_candidate_contract_test stable_candidate_c_test.c)
target_compile_features(granit_stable_candidate_contract_test PRIVATE c_std_11)
target_link_libraries(
  granit_stable_candidate_contract_test
  PRIVATE granit::granit granit::render_pipeline granit::window Unity::Unity
)
set_target_properties(
  granit_stable_candidate_contract_test PROPERTIES LINKER_LANGUAGE CXX
)
granit_target_compile_warnings(granit_stable_candidate_contract_test)
granit_target_output_directories(granit_stable_candidate_contract_test)
add_test(
  NAME granit.contracts.stable_candidate
  COMMAND granit_stable_candidate_contract_test
)
set_tests_properties(
  granit.contracts.stable_candidate
  PROPERTIES
    LABELS "unit;contract"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
)
