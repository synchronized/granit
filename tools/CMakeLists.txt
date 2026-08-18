# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(GRANIT_SPIRV_REFLECT_DIR "${PROJECT_SOURCE_DIR}/3rd/spirv-reflect-1.4.350.0")
set(GRANIT_SPIRV_HEADERS_DIR "${PROJECT_SOURCE_DIR}/3rd/spirv-headers-1.4.350.0/include")

add_library(granit_spirv_reflect STATIC "${GRANIT_SPIRV_REFLECT_DIR}/spirv_reflect.c")
target_compile_definitions(granit_spirv_reflect PUBLIC SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)
target_include_directories(
  granit_spirv_reflect
  BEFORE PUBLIC "${GRANIT_SPIRV_REFLECT_DIR}" "${GRANIT_SPIRV_HEADERS_DIR}"
)
set_target_properties(granit_spirv_reflect PROPERTIES FOLDER "ThirdParty/SPIRV-Reflect")

add_executable(granit_shader_tool shader_tool_main.cpp)
target_compile_features(granit_shader_tool PRIVATE cxx_std_20)
target_link_libraries(granit_shader_tool PRIVATE granit_spirv_reflect)
target_compile_options(
  granit_shader_tool
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_shader_tool)
granit_target_output_directories(granit_shader_tool)
set_target_properties(granit_shader_tool PROPERTIES FOLDER "Tools")

add_executable(
  granit_material_tool
  material_tool_main.cpp
  "${PROJECT_SOURCE_DIR}/src/material/material_debug_json.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_source_json.cpp"
)
target_compile_features(granit_material_tool PRIVATE cxx_std_20)
target_include_directories(granit_material_tool PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_material_tool PRIVATE granit::material)
target_compile_options(
  granit_material_tool
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_material_tool)
granit_target_output_directories(granit_material_tool)
set_target_properties(granit_material_tool PROPERTIES FOLDER "Tools")

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(
    NAME granit.shader_tool.tone_mapping_reflect
    COMMAND granit_shader_tool
            "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.tone_mapping_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,0,0,uniform_buffer,ToneMappingConstants,16.*binding,0,1,sampled_texture,hdr_color,1.*binding,0,2,sampler,hdr_sampler,1"
  )

  add_test(
    NAME granit.shader_tool.pbr_ibl_reflect
    COMMAND granit_shader_tool
            "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_ibl_untextured.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.pbr_ibl_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,3,uniform_buffer,IblConstants,16.*binding,3,4,sampled_texture,irradiance_texture,1.*binding,3,5,sampled_texture,prefiltered_environment_texture,1.*binding,3,6,sampled_texture,brdf_lut_texture,1.*binding,3,7,sampler,environment_sampler,1"
  )
  add_test(
    NAME granit.shader_tool.pbr_shadow_ibl_reflect
    COMMAND
      granit_shader_tool
      "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_untextured.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.pbr_shadow_ibl_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,0,uniform_buffer,ShadowConstants,80.*binding,3,1,sampled_texture,shadow_texture,1.*binding,3,2,sampler,shadow_sampler,1.*binding,3,3,uniform_buffer,IblConstants,16.*binding,3,4,sampled_texture,irradiance_texture,1.*binding,3,5,sampled_texture,prefiltered_environment_texture,1.*binding,3,6,sampled_texture,brdf_lut_texture,1.*binding,3,7,sampler,environment_sampler,1"
  )

  add_test(
    NAME granit.shader_tool.pbr_shadow_ibl_lights_reflect
    COMMAND
      granit_shader_tool
      "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights_untextured.frag.spv"
  )

  add_test(
    NAME granit.shader_tool.pbr_lights_reflect
    COMMAND granit_shader_tool
            "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_lights_untextured.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.pbr_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,8,uniform_buffer,LightCounts,16.*binding,3,9,storage_buffer,directional_lights,1.*binding,3,10,storage_buffer,point_lights,1.*binding,3,11,storage_buffer,spot_lights,1"
  )
  add_test(
    NAME granit.shader_tool.pbr_ibl_lights_reflect
    COMMAND granit_shader_tool
            "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_ibl_lights_untextured.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.pbr_ibl_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,3,uniform_buffer,IblConstants,16.*binding,3,4,sampled_texture,irradiance_texture,1.*binding,3,5,sampled_texture,prefiltered_environment_texture,1.*binding,3,6,sampled_texture,brdf_lut_texture,1.*binding,3,7,sampler,environment_sampler,1.*binding,3,8,uniform_buffer,LightCounts,16"
  )
  add_test(
    NAME granit.shader_tool.pbr_shadow_lights_reflect
    COMMAND granit_shader_tool
            "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_lights_untextured.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.pbr_shadow_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,0,uniform_buffer,ShadowConstants,80.*binding,3,1,sampled_texture,shadow_texture,1.*binding,3,2,sampler,shadow_sampler,1.*binding,3,8,uniform_buffer,LightCounts,16"
  )
  set_tests_properties(
    granit.shader_tool.pbr_shadow_ibl_lights_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,3,8,uniform_buffer,LightCounts,16.*binding,3,9,storage_buffer,directional_lights,1.*binding,3,10,storage_buffer,point_lights,1.*binding,3,11,storage_buffer,spot_lights,1"
  )

  add_test(
    NAME granit.shader_tool.unlit_reflect
    COMMAND granit_shader_tool
            "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit_alpha_cutoff.frag.spv"
  )
  set_tests_properties(
    granit.shader_tool.unlit_reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "binding,1,0,uniform_buffer,MaterialConstants,32"
  )

  set(GRANIT_MATERIAL_PACKAGE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/material_package_fixture.grmat")
  set(GRANIT_MATERIAL_DEBUG_FIXTURE "${GRANIT_MATERIAL_PACKAGE_FIXTURE}.debug.json")
  add_test(
    NAME granit.material_tool.build
    COMMAND
      granit_material_tool build
      "${PROJECT_SOURCE_DIR}/tests/assets/material_package_fixture.grmat.json"
      --output "${GRANIT_MATERIAL_PACKAGE_FIXTURE}" --emit-debug-json
  )
  set_tests_properties(granit.material_tool.build PROPERTIES FIXTURES_SETUP granit_material_package)
  add_test(
    NAME granit.material_tool.inspect
    COMMAND granit_material_tool inspect "${GRANIT_MATERIAL_PACKAGE_FIXTURE}" --json
  )
  set_tests_properties(
    granit.material_tool.inspect
    PROPERTIES
      FIXTURES_REQUIRED granit_material_package
      PASS_REGULAR_EXPRESSION "\"magic\": \"GRMAT\""
  )

  set(GRANIT_UNLIT_PACKAGE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/unlit.grmat")
  add_test(
    NAME granit.material_tool.unlit_build
    COMMAND
      granit_material_tool build "${PROJECT_SOURCE_DIR}/examples/assets/unlit.grmat.json"
      --output "${GRANIT_UNLIT_PACKAGE_FIXTURE}"
  )
  set_tests_properties(granit.material_tool.unlit_build PROPERTIES FIXTURES_SETUP granit_unlit)
  add_test(
    NAME granit.material_tool.unlit_inspect
    COMMAND granit_material_tool inspect "${GRANIT_UNLIT_PACKAGE_FIXTURE}" --json
  )
  set_tests_properties(
    granit.material_tool.unlit_inspect
    PROPERTIES
      FIXTURES_REQUIRED granit_unlit
      PASS_REGULAR_EXPRESSION "\"constant_buffer_size\": 32"
  )
endif()

find_program(
  GRANIT_DXC_EXECUTABLE
  NAMES dxc
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
)
if(GRANIT_DXC_EXECUTABLE)
  set(GRANIT_DXC_PROBE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dxc_spirv_probe.spv")
  execute_process(
    COMMAND
      "${GRANIT_DXC_EXECUTABLE}" -spirv -fspv-target-env=vulkan1.3 -T ps_6_6
      -E fragment_main -Fo "${GRANIT_DXC_PROBE_OUTPUT}"
      "${PROJECT_SOURCE_DIR}/tests/assets/material_fixture.hlsl"
    RESULT_VARIABLE GRANIT_DXC_PROBE_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
  )
  file(REMOVE "${GRANIT_DXC_PROBE_OUTPUT}")
  if(NOT GRANIT_DXC_PROBE_RESULT EQUAL 0)
    message(STATUS "检测到的 dxc 不支持 SPIR-V，跳过依赖该能力的测试")
    unset(GRANIT_DXC_EXECUTABLE)
  endif()
endif()
find_program(
  GRANIT_SPIRV_VAL_EXECUTABLE
  NAMES spirv-val
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING AND GRANIT_DXC_EXECUTABLE)
  set(GRANIT_MATERIAL_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/material_fixture.spv")
  add_custom_command(
    OUTPUT "${GRANIT_MATERIAL_FIXTURE}"
    COMMAND
      "${GRANIT_DXC_EXECUTABLE}" -spirv -fspv-target-env=vulkan1.3 -T ps_6_6
      -E fragment_main -Fo "${GRANIT_MATERIAL_FIXTURE}"
      "${PROJECT_SOURCE_DIR}/tests/assets/material_fixture.hlsl"
    DEPENDS "${PROJECT_SOURCE_DIR}/tests/assets/material_fixture.hlsl"
    COMMENT "使用 DXC 编译材质 HLSL fixture"
    VERBATIM
  )
  add_custom_target(granit_material_shader_fixture DEPENDS "${GRANIT_MATERIAL_FIXTURE}")
  add_dependencies(granit_shader_tool granit_material_shader_fixture)

  if(GRANIT_SPIRV_VAL_EXECUTABLE)
    add_test(
      NAME granit.shader_tool.validate
      COMMAND "${GRANIT_SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.3
              "${GRANIT_MATERIAL_FIXTURE}"
    )
  endif()
  add_test(NAME granit.shader_tool.reflect COMMAND granit_shader_tool "${GRANIT_MATERIAL_FIXTURE}")
  set_tests_properties(
    granit.shader_tool.reflect
    PROPERTIES PASS_REGULAR_EXPRESSION
               "entry,fragment_main,fragment.*binding,1,0,uniform_buffer,MaterialConstants,32.*binding,1,1,sampled_texture,albedo_texture,1.*binding,1,2,sampler,linear_sampler,1"
  )
elseif(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  message(WARNING "未找到 dxc，跳过 Granit Shader 工具端到端测试")
endif()
