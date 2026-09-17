# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS integration tooling)

find_package(Threads REQUIRED)

if(GRANIT_TESTING_ENABLED)

  add_executable(
    granit_asset_tools_handle_safety_test
    "${CMAKE_CURRENT_SOURCE_DIR}/handle_safety_test.c"
  )
  target_compile_features(granit_asset_tools_handle_safety_test PRIVATE c_std_11)
  target_link_libraries(granit_asset_tools_handle_safety_test PRIVATE granit::asset_tools)
  set_target_properties(granit_asset_tools_handle_safety_test PROPERTIES LINKER_LANGUAGE CXX)
  granit_target_compile_warnings(granit_asset_tools_handle_safety_test)
  granit_target_output_directories(granit_asset_tools_handle_safety_test)
  add_test(
    NAME granit.asset_tools.handle_safety
    COMMAND granit_asset_tools_handle_safety_test
  )

  add_executable(
    granit_asset_tools_material_test
    "${CMAKE_CURRENT_SOURCE_DIR}/material/api_test.cpp"
  )
  target_compile_features(granit_asset_tools_material_test PRIVATE cxx_std_20)
  target_link_libraries(granit_asset_tools_material_test PRIVATE granit::asset_tools)
  granit_target_compile_warnings(granit_asset_tools_material_test)
  granit_target_output_directories(granit_asset_tools_material_test)
  add_test(
    NAME granit.asset_tools.material.api
    COMMAND
      granit_asset_tools_material_test
      "${granit_test_material_fixture_dir}/material_package_fixture.grmat.json"
      "${granit_test_shader_fixture_dir}/material_fixture.grshidx.json"
  )

  add_executable(
    granit_asset_tools_material_c_test
    "${CMAKE_CURRENT_SOURCE_DIR}/material/c_api_test.c"
  )
  target_compile_features(granit_asset_tools_material_c_test PRIVATE c_std_11)
  target_link_libraries(granit_asset_tools_material_c_test PRIVATE granit::asset_tools)
  set_target_properties(granit_asset_tools_material_c_test PROPERTIES LINKER_LANGUAGE CXX)
  granit_target_compile_warnings(granit_asset_tools_material_c_test)
  granit_target_output_directories(granit_asset_tools_material_c_test)
  add_test(
    NAME granit.asset_tools.material.c_api
    COMMAND
      granit_asset_tools_material_c_test
      "${granit_test_material_fixture_dir}/material_package_fixture.grmat.json"
      "${granit_test_shader_fixture_dir}/material_fixture.grshidx.json"
  )

  add_executable(
    granit_asset_tools_texture_test
    "${CMAKE_CURRENT_SOURCE_DIR}/texture/api_test.cpp"
  )
  target_compile_features(granit_asset_tools_texture_test PRIVATE cxx_std_20)
  target_link_libraries(granit_asset_tools_texture_test PRIVATE granit::asset_tools)
  granit_target_compile_warnings(granit_asset_tools_texture_test)
  granit_target_output_directories(granit_asset_tools_texture_test)
  add_test(NAME granit.asset_tools.texture.api COMMAND granit_asset_tools_texture_test)

  add_executable(
    granit_asset_tools_texture_c_test
    "${CMAKE_CURRENT_SOURCE_DIR}/texture/c_api_test.c"
  )
  target_compile_features(granit_asset_tools_texture_c_test PRIVATE c_std_11)
  target_link_libraries(granit_asset_tools_texture_c_test PRIVATE granit::asset_tools)
  set_target_properties(granit_asset_tools_texture_c_test PROPERTIES LINKER_LANGUAGE CXX)
  granit_target_compile_warnings(granit_asset_tools_texture_c_test)
  granit_target_output_directories(granit_asset_tools_texture_c_test)
  add_test(NAME granit.asset_tools.texture.c_api COMMAND granit_asset_tools_texture_c_test)
  add_test(
    NAME granit.asset_tools.texture.cli
    COMMAND
      "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:granit_asset_tool>"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/texture-cli-test" -P
      "${PROJECT_SOURCE_DIR}/cmake/check_texture_asset_tool.cmake"
  )

  add_executable(
    granit_asset_tools_environment_test
    "${CMAKE_CURRENT_SOURCE_DIR}/environment/api_test.cpp"
  )
  target_compile_features(granit_asset_tools_environment_test PRIVATE cxx_std_20)
  target_link_libraries(granit_asset_tools_environment_test PRIVATE granit::asset_tools)
  granit_target_compile_warnings(granit_asset_tools_environment_test)
  granit_target_output_directories(granit_asset_tools_environment_test)
  add_test(NAME granit.asset_tools.environment.api COMMAND granit_asset_tools_environment_test)

  add_executable(
    granit_asset_tools_environment_c_test
    "${CMAKE_CURRENT_SOURCE_DIR}/environment/c_api_test.c"
  )
  target_compile_features(granit_asset_tools_environment_c_test PRIVATE c_std_11)
  target_link_libraries(granit_asset_tools_environment_c_test PRIVATE granit::asset_tools)
  set_target_properties(granit_asset_tools_environment_c_test PROPERTIES LINKER_LANGUAGE CXX)
  granit_target_compile_warnings(granit_asset_tools_environment_c_test)
  granit_target_output_directories(granit_asset_tools_environment_c_test)
  add_test(NAME granit.asset_tools.environment.c_api COMMAND granit_asset_tools_environment_c_test)
  add_test(
    NAME granit.asset_tools.environment.cli
    COMMAND
      "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:granit_asset_tool>"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/environment-cli-test"
      "-DFIXTURE=${granit_installed_asset_snapshot_dir}/environments/studio_small_03.grenv" -P
      "${PROJECT_SOURCE_DIR}/cmake/check_environment_asset_tool.cmake"
  )

  set(granit_shader_fixture_object_dir "${CMAKE_CURRENT_BINARY_DIR}/test-objects")
  set(granit_shader_fixture_object_outputs)
  foreach(stage IN ITEMS vert frag)
    if(stage STREQUAL "vert")
      set(shader_stage vertex)
    else()
      set(shader_stage fragment)
    endif()
    granit_add_test_shader_object_from_payloads(
      NAME "minimal.${stage}"
      SPIRV "${granit_test_shader_fixture_dir}/minimal.${stage}.spv"
      WGSL "${granit_test_shader_fixture_dir}/minimal.${stage}.wgsl"
      ENTRY main
      STAGE "${shader_stage}"
      OUTPUT_DIR "${granit_shader_fixture_object_dir}"
      OUTPUT_VAR object_outputs
    )
    list(APPEND granit_shader_fixture_object_outputs ${object_outputs})
  endforeach()
  add_custom_target(
    granit_shader_fixture_objects ALL DEPENDS ${granit_shader_fixture_object_outputs}
  )

  add_executable(
    granit_asset_tools_shader_object_codec_test
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/object_codec_test.cpp"
    $<TARGET_OBJECTS:granit_asset_tools_shader_object_storage>
    $<TARGET_OBJECTS:granit_asset_format_shader>
    $<TARGET_OBJECTS:granit_asset_tools_shader_compiler>
    $<TARGET_OBJECTS:granit_spirv_reflect>
  )
  target_compile_features(granit_asset_tools_shader_object_codec_test PRIVATE cxx_std_20)
  target_include_directories(
    granit_asset_tools_shader_object_codec_test
    PRIVATE "${PROJECT_SOURCE_DIR}/tools" "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/include"
  )
  target_link_libraries(granit_asset_tools_shader_object_codec_test PRIVATE Threads::Threads)
  granit_target_compile_warnings(granit_asset_tools_shader_object_codec_test)
  granit_target_output_directories(granit_asset_tools_shader_object_codec_test)
  add_test(
    NAME granit.asset_tools.shader.object_codec
    COMMAND
      granit_asset_tools_shader_object_codec_test
      "${CMAKE_CURRENT_BINARY_DIR}/shader-object-test-cache"
  )

  add_executable(
    granit_asset_tools_shader_library_codec_test
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/library_codec_test.cpp"
    $<TARGET_OBJECTS:granit_asset_format_shader>
  )
  target_compile_features(granit_asset_tools_shader_library_codec_test PRIVATE cxx_std_20)
  target_include_directories(
    granit_asset_tools_shader_library_codec_test
    PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
  )
  granit_target_compile_warnings(granit_asset_tools_shader_library_codec_test)
  granit_target_output_directories(granit_asset_tools_shader_library_codec_test)
  add_test(
    NAME granit.asset_tools.shader.library_codec
    COMMAND granit_asset_tools_shader_library_codec_test
  )

  add_executable(
    granit_asset_tools_shader_library_source_manifest_test
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/library_source_manifest_test.cpp"
    $<TARGET_OBJECTS:granit_asset_tools_shader_library_source>
  )
  target_compile_features(granit_asset_tools_shader_library_source_manifest_test PRIVATE cxx_std_20)
  target_include_directories(
    granit_asset_tools_shader_library_source_manifest_test
    PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
  )
  target_link_libraries(
    granit_asset_tools_shader_library_source_manifest_test PRIVATE Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_asset_tools_shader_library_source_manifest_test)
  granit_target_output_directories(granit_asset_tools_shader_library_source_manifest_test)
  add_test(
    NAME granit.asset_tools.shader.library_source_manifest
    COMMAND granit_asset_tools_shader_library_source_manifest_test
  )

  if(TARGET granit_asset_tools)
    add_executable(
      granit_asset_tools_shader_c_test "${CMAKE_CURRENT_SOURCE_DIR}/shader/c_api_test.c"
    )
    target_compile_features(granit_asset_tools_shader_c_test PRIVATE c_std_11)
    target_link_libraries(granit_asset_tools_shader_c_test PRIVATE granit::asset_tools)
    set_target_properties(granit_asset_tools_shader_c_test PROPERTIES LINKER_LANGUAGE CXX)
    granit_target_compile_warnings(granit_asset_tools_shader_c_test)
    granit_target_output_directories(granit_asset_tools_shader_c_test)
    add_test(
      NAME granit.asset_tools.shader.c_api
      COMMAND granit_asset_tools_shader_c_test
              "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.frag.spv"
    )

    add_executable(
      granit_asset_tools_shader_cpp_test "${CMAKE_CURRENT_SOURCE_DIR}/shader/cpp_api_test.cpp"
    )
    target_compile_features(granit_asset_tools_shader_cpp_test PRIVATE cxx_std_20)
    target_link_libraries(granit_asset_tools_shader_cpp_test PRIVATE granit::asset_tools)
    granit_target_compile_warnings(granit_asset_tools_shader_cpp_test)
    granit_target_output_directories(granit_asset_tools_shader_cpp_test)
    add_test(
      NAME granit.asset_tools.shader.cpp_api
      COMMAND granit_asset_tools_shader_cpp_test
              "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.frag.spv"
              "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/unlit/unlit.vert.spv"
              "${granit_test_shader_fixture_dir}/minimal.comp.spv"
    )

  endif()

  add_test(
    NAME granit.asset_tools.shader.tone_mapping_reflect
    COMMAND granit_asset_tool shader inspect
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.tone_mapping_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,0,0,uniform_buffer,ToneMappingConstants,16.*binding,0,1,sampled_texture,hdr_color,1.*binding,0,2,sampler,hdr_sampler,1"
  )
  add_test(
    NAME granit.asset_tools.shader.tone_mapping_json
    COMMAND granit_asset_tool shader inspect --json
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.tone_mapping_json
    PROPERTIES PASS_REGULAR_EXPRESSION
               "\"schema\": 1.*\"entry_point\": \"fragment_main\".*\"group\": 0, \"binding\": 0, \"type\": \"uniform_buffer\", \"access\": \"read\""
  )
  add_test(
    NAME granit.asset_tools.shader.verify
    COMMAND
      granit_asset_tool shader verify
      "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.verify
    PROPERTIES PASS_REGULAR_EXPRESSION "SPIR-V 结构验证通过.*fragment_main.*fragment"
  )
  add_test(
    NAME granit.asset_tools.shader.reject_fixture_command
    COMMAND granit_asset_tool shader fixture-object
  )
  set_tests_properties(granit.asset_tools.shader.reject_fixture_command PROPERTIES WILL_FAIL TRUE)
  add_test(
    NAME granit.asset_tools.reject_ungrouped_shader_command
    COMMAND granit_asset_tool inspect
  )
  set_tests_properties(granit.asset_tools.reject_ungrouped_shader_command PROPERTIES WILL_FAIL TRUE)
  set(granit_shader_object_id_output
      "${CMAKE_CURRENT_BINARY_DIR}/object-id-test/minimal_shader_ids.inc")
  add_test(
    NAME granit.shader_fixture_tool.object_ids
    COMMAND granit_shader_fixture_tool object-ids
            --object
            "minimal_vertex_id=${granit_shader_fixture_object_dir}/minimal.vert.grshaderobj"
            --output "${granit_shader_object_id_output}"
  )
  add_test(
    NAME granit.shader_fixture_tool.object_ids_compare
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${granit_shader_object_id_output}"
            "${granit_test_shader_fixture_dir}/minimal_shader_ids.inc"
  )
  set_tests_properties(granit.shader_fixture_tool.object_ids_compare
                       PROPERTIES DEPENDS granit.shader_fixture_tool.object_ids)
  set(granit_shader_index_id_output
      "${CMAKE_CURRENT_BINARY_DIR}/index-id-test/material_shader_ids.inc")
  add_test(
    NAME granit.asset_tools.shader.index_ids
    COMMAND granit_asset_tool shader index-ids
            --index "${granit_test_shader_fixture_dir}/material_fixture.grshidx.json"
            --shader material_fragment_id=main.fragment
            --output "${granit_shader_index_id_output}"
  )
  add_test(
    NAME granit.shader_fixture_tool.library
    COMMAND
      granit_shader_fixture_tool library
      --object "${granit_shader_fixture_object_dir}/minimal.vert.grshaderobj"
      --object "${granit_shader_fixture_object_dir}/minimal.frag.grshaderobj"
      --target all
      --output "${CMAKE_CURRENT_BINARY_DIR}/library-test/minimal.grshlib"
  )
  set_tests_properties(
    granit.shader_fixture_tool.library
    PROPERTIES PASS_REGULAR_EXPRESSION "Shader Library"
  )
  add_test(
    NAME granit.asset_tools.shader.compile_diagnostic
    COMMAND
      "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:granit_asset_tool>"
      "-DASSET_TOOLS_LIBRARY=$<TARGET_FILE:granit_asset_tools>"
      "-DINPUT=${granit_asset_sources_dir}/shaders/pbr/pbr_untextured.hlsl"
      "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/must-not-exist.spv" -P
      "${PROJECT_SOURCE_DIR}/cmake/check_shader_tool_diagnostic.cmake"
  )

  add_test(
    NAME granit.asset_tools.shader.pbr_ibl_reflect
    COMMAND granit_asset_tool shader inspect
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/pbr_ibl_untextured.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.pbr_ibl_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,3,uniform_buffer,IblConstants,16.*binding,3,4,sampled_texture,irradiance_texture,1.*binding,3,5,sampled_texture,prefiltered_environment_texture,1.*binding,3,6,sampled_texture,brdf_lut_texture,1.*binding,3,7,sampler,environment_sampler,1"
  )
  add_test(
    NAME granit.asset_tools.shader.pbr_shadow_ibl_reflect
    COMMAND
      granit_asset_tool shader inspect
      "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/pbr_shadow_ibl_untextured.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.pbr_shadow_ibl_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,0,uniform_buffer,ShadowConstants,80.*binding,3,1,sampled_texture,shadow_texture,1.*binding,3,2,sampler,shadow_sampler,1.*binding,3,3,uniform_buffer,IblConstants,16.*binding,3,4,sampled_texture,irradiance_texture,1.*binding,3,5,sampled_texture,prefiltered_environment_texture,1.*binding,3,6,sampled_texture,brdf_lut_texture,1.*binding,3,7,sampler,environment_sampler,1"
  )

  add_test(
    NAME granit.asset_tools.shader.pbr_shadow_ibl_lights_reflect
    COMMAND
      granit_asset_tool shader inspect
      "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/pbr_shadow_ibl_lights_untextured.frag.spv"
  )

  add_test(
    NAME granit.asset_tools.shader.pbr_lights_reflect
    COMMAND granit_asset_tool shader inspect
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/pbr_lights_untextured.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.pbr_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,8,uniform_buffer,LightCounts,16.*binding,3,9,storage_buffer,directional_lights,1.*binding,3,10,storage_buffer,point_lights,1.*binding,3,11,storage_buffer,spot_lights,1"
  )
  add_test(
    NAME granit.asset_tools.shader.pbr_ibl_lights_reflect
    COMMAND granit_asset_tool shader inspect
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/pbr_ibl_lights_untextured.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.pbr_ibl_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,3,uniform_buffer,IblConstants,16.*binding,3,4,sampled_texture,irradiance_texture,1.*binding,3,5,sampled_texture,prefiltered_environment_texture,1.*binding,3,6,sampled_texture,brdf_lut_texture,1.*binding,3,7,sampler,environment_sampler,1.*binding,3,8,uniform_buffer,LightCounts,16"
  )
  add_test(
    NAME granit.asset_tools.shader.pbr_shadow_lights_reflect
    COMMAND granit_asset_tool shader inspect
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pbr/pbr_shadow_lights_untextured.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.pbr_shadow_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,0,uniform_buffer,ShadowConstants,80.*binding,3,1,sampled_texture,shadow_texture,1.*binding,3,2,sampler,shadow_sampler,1.*binding,3,8,uniform_buffer,LightCounts,16"
  )
  set_tests_properties(
    granit.asset_tools.shader.pbr_shadow_ibl_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,8,uniform_buffer,LightCounts,16.*binding,3,9,storage_buffer,directional_lights,1.*binding,3,10,storage_buffer,point_lights,1.*binding,3,11,storage_buffer,spot_lights,1"
  )

  add_test(
    NAME granit.asset_tools.shader.unlit_reflect
    COMMAND granit_asset_tool shader inspect
            "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/unlit/unlit_alpha_cutoff.frag.spv"
  )
  set_tests_properties(
    granit.asset_tools.shader.unlit_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,1,0,uniform_buffer,MaterialConstants,32"
  )

  set(GRANIT_MATERIAL_PACKAGE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/material_package_fixture.grmat")
  set(GRANIT_MATERIAL_DEBUG_FIXTURE "${GRANIT_MATERIAL_PACKAGE_FIXTURE}.debug.json")
  add_test(
    NAME granit.asset_tools.material.build
    COMMAND
      granit_asset_tool material build
      "${granit_test_material_fixture_dir}/material_package_fixture.grmat.json"
      --output "${GRANIT_MATERIAL_PACKAGE_FIXTURE}" --shader-index
      "${granit_test_shader_fixture_dir}/material_fixture.grshidx.json" --emit-debug-json
  )
  set_tests_properties(granit.asset_tools.material.build PROPERTIES FIXTURES_SETUP granit_material_package)
  add_test(
    NAME granit.asset_tools.material.inspect
    COMMAND granit_asset_tool material inspect "${GRANIT_MATERIAL_PACKAGE_FIXTURE}" --json
  )
  set_tests_properties(
    granit.asset_tools.material.inspect
    PROPERTIES
      FIXTURES_REQUIRED granit_material_package
      PASS_REGULAR_EXPRESSION "\"magic\": \"GRMAT\""
  )

  set(GRANIT_UNLIT_PACKAGE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/unlit.grmat")
  add_test(
    NAME granit.asset_tools.material.unlit_build
    COMMAND
      granit_asset_tool material build "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/unlit.grmat.json"
      --output "${GRANIT_UNLIT_PACKAGE_FIXTURE}" --shader-index
      "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke/unlit.grshidx.json"
  )
  set_tests_properties(granit.asset_tools.material.unlit_build PROPERTIES FIXTURES_SETUP granit_unlit)
  add_test(
    NAME granit.asset_tools.material.unlit_inspect
    COMMAND granit_asset_tool material inspect "${GRANIT_UNLIT_PACKAGE_FIXTURE}" --json
  )
  set_tests_properties(
    granit.asset_tools.material.unlit_inspect
    PROPERTIES
      FIXTURES_REQUIRED granit_unlit
      PASS_REGULAR_EXPRESSION "\"constant_buffer_size\": 48"
  )
endif()

if(GRANIT_DXC_EXECUTABLE AND NOT GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "unchecked")
  set(GRANIT_DXC_PROBE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dxc_spirv_probe.spv")
  execute_process(
    COMMAND
      "${GRANIT_DXC_EXECUTABLE}" -spirv -fspv-target-env=vulkan1.3 -T ps_6_6
      -E fragment_main -Fo "${GRANIT_DXC_PROBE_OUTPUT}"
      "${granit_test_shader_fixture_dir}/material_fixture.hlsl"
    RESULT_VARIABLE GRANIT_DXC_PROBE_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
  )
  file(REMOVE "${GRANIT_DXC_PROBE_OUTPUT}")
  if(NOT GRANIT_DXC_PROBE_RESULT EQUAL 0)
    message(STATUS "检测到的 dxc 不支持 SPIR-V，跳过依赖该能力的测试")
    unset(GRANIT_DXC_EXECUTABLE)
    unset(GRANIT_DXC_EXECUTABLE CACHE)
  endif()
endif()
find_program(
  GRANIT_SPIRV_VAL_EXECUTABLE
  NAMES spirv-val
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
)

if(GRANIT_TESTING_ENABLED AND GRANIT_DXC_EXECUTABLE)
  set(GRANIT_MATERIAL_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/material_fixture.spv")
  add_custom_command(
    OUTPUT "${GRANIT_MATERIAL_FIXTURE}"
    COMMAND
      "${GRANIT_DXC_EXECUTABLE}" -spirv -fspv-target-env=vulkan1.3 -T ps_6_6
      -E fragment_main -Fo "${GRANIT_MATERIAL_FIXTURE}"
      "${granit_test_shader_fixture_dir}/material_fixture.hlsl"
    DEPENDS "${granit_test_shader_fixture_dir}/material_fixture.hlsl"
    COMMENT "使用 DXC 编译材质 HLSL fixture"
    VERBATIM
  )
  add_custom_target(granit_material_shader_fixture DEPENDS "${GRANIT_MATERIAL_FIXTURE}")
  add_dependencies(granit_asset_tool granit_material_shader_fixture)

  if(GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_TINT_EXECUTABLE)
    add_executable(
      granit_asset_tools_shader_hlsl_api_test
      "${CMAKE_CURRENT_SOURCE_DIR}/shader/hlsl_api_test.cpp"
    )
    target_compile_features(granit_asset_tools_shader_hlsl_api_test PRIVATE cxx_std_20)
    target_link_libraries(granit_asset_tools_shader_hlsl_api_test PRIVATE granit::asset_tools)
    granit_target_compile_warnings(granit_asset_tools_shader_hlsl_api_test)
    granit_target_output_directories(granit_asset_tools_shader_hlsl_api_test)
    add_test(
      NAME granit.asset_tools.shader.hlsl_api
      COMMAND
        granit_asset_tools_shader_hlsl_api_test "${GRANIT_SHADER_TOOLCHAIN_ROOT}"
        "${granit_test_shader_fixture_dir}/material_fixture.hlsl"
        "${CMAKE_CURRENT_BINARY_DIR}/shader-hlsl-api-test"
    )
  endif()

  if(GRANIT_SPIRV_VAL_EXECUTABLE)
    add_test(
      NAME granit.asset_tools.shader.validate
      COMMAND "${GRANIT_SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.3
              "${GRANIT_MATERIAL_FIXTURE}"
    )
  endif()
  add_test(
    NAME granit.asset_tools.shader.reflect
    COMMAND granit_asset_tool shader inspect "${GRANIT_MATERIAL_FIXTURE}"
  )
  set_tests_properties(
    granit.asset_tools.shader.reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "entry,fragment_main,fragment.*binding,1,0,uniform_buffer,MaterialConstants,32.*binding,1,1,sampled_texture,albedo_texture,1.*binding,1,2,sampler,linear_sampler,1"
  )
  add_test(
    NAME granit.asset_tools.shader.cpp_override_api
    COMMAND
      granit_asset_tools_shader_cpp_test
      "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/pipeline/tone_mapping.frag.spv"
      "${PROJECT_SOURCE_DIR}/tests/fixtures/generated/unlit/unlit.vert.spv"
      "${granit_test_shader_fixture_dir}/minimal.comp.spv" "${GRANIT_MATERIAL_FIXTURE}"
  )
elseif(GRANIT_TESTING_ENABLED)
  message(WARNING "未找到 dxc，跳过 Granit Shader 工具端到端测试")
endif()

if(GRANIT_TESTING_ENABLED)
  add_test(
    NAME granit.shader_toolchain.manifest
    COMMAND
      "${CMAKE_COMMAND}"
      -DGENERATOR=${PROJECT_SOURCE_DIR}/cmake/generate_shader_toolchain_manifest.cmake
      -DVERIFIER=${PROJECT_SOURCE_DIR}/cmake/verify_shader_toolchain_manifest.cmake
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-toolchain-manifest-test -P
      "${PROJECT_SOURCE_DIR}/cmake/check_shader_toolchain_manifest.cmake"
  )
  add_test(
    NAME granit.shader_toolchain.package
    COMMAND
      "${CMAKE_COMMAND}"
      -DPACKAGER=${PROJECT_SOURCE_DIR}/cmake/package_shader_toolchain.cmake
      -DGENERATOR=${PROJECT_SOURCE_DIR}/cmake/generate_shader_toolchain_manifest.cmake
      -DVERIFIER=${PROJECT_SOURCE_DIR}/cmake/verify_shader_toolchain_manifest.cmake
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-toolchain-package-test -P
      "${PROJECT_SOURCE_DIR}/cmake/check_package_shader_toolchain.cmake"
  )
  add_test(
    NAME granit.shader_toolchain.licenses
    COMMAND
      "${CMAKE_COMMAND}"
      -DCOLLECTOR=${PROJECT_SOURCE_DIR}/cmake/collect_license_bundle.cmake
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-toolchain-license-test -P
      "${PROJECT_SOURCE_DIR}/cmake/check_collect_license_bundle.cmake"
  )
  add_test(
    NAME granit.shader_toolchain.modes
    COMMAND
      "${CMAKE_COMMAND}"
      "-DMODULE=${PROJECT_SOURCE_DIR}/cmake/GranitShaderToolchain.cmake"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-toolchain-mode-test" -P
      "${PROJECT_SOURCE_DIR}/cmake/check_shader_toolchain_modes.cmake"
  )
  set_property(
    TEST
      granit.shader_toolchain.manifest
      granit.shader_toolchain.package
      granit.shader_toolchain.licenses
      granit.shader_toolchain.modes
    APPEND PROPERTY LABELS package release
  )

  if(GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND GRANIT_TINT_EXECUTABLE)
    add_executable(
      granit_asset_tools_shader_source_library_builder_api_test
      "${CMAKE_CURRENT_SOURCE_DIR}/shader/source_library_builder_api_test.cpp"
    )
    target_compile_features(
      granit_asset_tools_shader_source_library_builder_api_test PRIVATE cxx_std_20
    )
    target_link_libraries(
      granit_asset_tools_shader_source_library_builder_api_test PRIVATE granit::asset_tools
    )
    granit_target_compile_warnings(granit_asset_tools_shader_source_library_builder_api_test)
    granit_target_output_directories(granit_asset_tools_shader_source_library_builder_api_test)
    add_test(
      NAME granit.asset_tools.shader.source_library_builder_api
      COMMAND
        granit_asset_tools_shader_source_library_builder_api_test "${GRANIT_SHADER_TOOLCHAIN_ROOT}"
        "${granit_test_shader_fixture_dir}/material_fixture.grshlib.json"
        "${CMAKE_CURRENT_BINARY_DIR}/source-library-builder-api"
    )
    add_test(
      NAME granit.asset_tools.shader.build_library
      COMMAND
        granit_asset_tool shader build-library --manifest
        "${granit_test_shader_fixture_dir}/material_fixture.grshlib.json" --toolchain
        "${GRANIT_SHADER_TOOLCHAIN_ROOT}" --cache
        "${CMAKE_CURRENT_BINARY_DIR}/source-library-cli/objects" --output
        "${CMAKE_CURRENT_BINARY_DIR}/source-library-cli/fixture.grshlib" --index
        "${CMAKE_CURRENT_BINARY_DIR}/source-library-cli/fixture.grshidx.json"
    )
  endif()
endif()
