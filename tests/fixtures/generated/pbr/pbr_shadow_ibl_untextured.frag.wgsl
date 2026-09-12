struct type_u002e_IblConstants {
    environment_rotation_cos: f32,
    environment_rotation_sin: f32,
    environment_intensity: f32,
    prefiltered_environment_max_mip: f32,
}

struct type_u002e_ShadowConstants {
    light_view_projection: mat4x4<f32>,
    shadow_depth_bias: f32,
    shadow_normal_bias: f32,
    shadow_texel_size: vec2<f32>,
}

struct type_u002e_MaterialConstants {
    base_color: vec4<f32>,
    metallic: f32,
    perceptual_roughness: f32,
    normal_scale: f32,
    occlusion_strength: f32,
    emissive: vec3<f32>,
    reserved_value: f32,
}

@group(3) @binding(3)
var<uniform> IblConstants: type_u002e_IblConstants;
@group(3) @binding(4)
var irradiance_texture: texture_cube<f32>;
@group(3) @binding(5)
var prefiltered_environment_texture: texture_cube<f32>;
@group(3) @binding(6)
var brdf_lut_texture: texture_2d<f32>;
@group(3) @binding(7)
var environment_sampler: sampler;
@group(3) @binding(0)
var<uniform> ShadowConstants: type_u002e_ShadowConstants;
@group(3) @binding(1)
var shadow_texture: texture_depth_2d;
@group(3) @binding(2)
var shadow_sampler: sampler_comparison;
@group(1) @binding(0)
var<uniform> MaterialConstants: type_u002e_MaterialConstants;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> in_u002e_var_u002e_TEXCOORD3_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_126_: bool;
    var phi_132_: bool;
    var phi_138_: bool;
    var phi_149_: f32;
    var phi_195_: vec3<f32>;

    let _e42 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e43 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e45 = MaterialConstants.base_color;
    let _e47 = MaterialConstants.metallic;
    let _e49 = MaterialConstants.perceptual_roughness;
    let _e50 = normalize(_e42);
    let _e52 = clamp(_e50.z, 0f, 1f);
    let _e53 = clamp(_e47, 0f, 1f);
    let _e54 = _e45.xyz;
    let _e56 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e54, vec3(_e53));
    let _e58 = MaterialConstants.occlusion_strength;
    switch bitcast<i32>(0u) {
        default: {
            let _e63 = ShadowConstants.shadow_normal_bias;
            let _e65 = (_e43 + (_e50 * _e63));
            let _e67 = ShadowConstants.light_view_projection;
            let _e73 = (vec4<f32>(_e65.x, _e65.y, _e65.z, 1f) * transpose(_e67));
            if (_e73.w <= 0f) {
                phi_149_ = 1f;
                break;
            }
            let _e78 = (_e73.xyz / vec3(_e73.w));
            let _e81 = ((_e78.xy * vec2<f32>(0.5f, -0.5f)) + vec2<f32>(0.5f, 0.5f));
            phi_126_ = true;
            if !(any((_e81 < vec2<f32>(0f, 0f)))) {
                phi_126_ = any((_e81 > vec2<f32>(1f, 1f)));
            }
            let _e88 = phi_126_;
            phi_132_ = true;
            if !(_e88) {
                phi_132_ = (_e78.z < 0f);
            }
            let _e93 = phi_132_;
            phi_138_ = true;
            if !(_e93) {
                phi_138_ = (_e78.z > 1f);
            }
            let _e98 = phi_138_;
            if _e98 {
                phi_149_ = 1f;
                break;
            }
            let _e101 = ShadowConstants.shadow_depth_bias;
            let _e103 = textureSampleCompareLevel(shadow_texture, shadow_sampler, _e81, (_e78.z - _e101));
            phi_149_ = _e103;
            break;
        }
    }
    let _e105 = phi_149_;
    switch bitcast<i32>(0u) {
        default: {
            if (_e52 <= 0f) {
                phi_195_ = vec3<f32>(0f, 0f, 0f);
                break;
            }
            let _e108 = normalize(vec3<f32>(0f, 0f, 2f));
            let _e116 = (_e56 + ((vec3<f32>(1f, 1f, 1f) - _e56) * pow((1f - clamp(clamp(_e108.z, 0f, 1f), 0f, 1f)), 5f)));
            let _e118 = clamp(dot(_e50, _e108), 0f, 1f);
            let _e120 = max(clamp(_e49, 0f, 1f), 0.045f);
            let _e121 = (_e120 * _e120);
            let _e122 = (_e121 * _e121);
            let _e126 = (((_e118 * _e118) * (_e122 - 1f)) + 1f);
            let _e135 = (_e52 * sqrt((((_e52 * _e52) * (1f - _e122)) + _e122)));
            phi_195_ = ((((((_e54 * (vec3<f32>(1f, 1f, 1f) - _e116)) * (1f - _e53)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e116 * (_e122 / ((3.1415927f * _e126) * _e126))) * (0.5f / (_e135 + _e135)))) * _e52) * _e105);
            break;
        }
    }
    let _e149 = phi_195_;
    let _e150 = clamp(_e49, 0f, 1f);
    let _e159 = (_e56 + ((max(vec3((1f - _e150)), _e56) - _e56) * pow((1f - clamp(_e52, 0f, 1f)), 5f)));
    let _e164 = IblConstants.environment_rotation_cos;
    let _e168 = IblConstants.environment_rotation_sin;
    let _e172 = -(_e168);
    let _e177 = textureSample(irradiance_texture, environment_sampler, vec3<f32>(((_e164 * _e50.x) + (_e168 * _e50.z)), _e50.y, ((_e172 * _e50.x) + (_e164 * _e50.z))));
    let _e179 = reflect(vec3<f32>(-0f, -0f, -1f), _e50);
    let _e191 = IblConstants.prefiltered_environment_max_mip;
    let _e193 = textureSampleLevel(prefiltered_environment_texture, environment_sampler, vec3<f32>(((_e164 * _e179.x) + (_e168 * _e179.z)), _e179.y, ((_e172 * _e179.x) + (_e164 * _e179.z))), (_e150 * _e191));
    let _e196 = textureSample(brdf_lut_texture, environment_sampler, vec2<f32>(_e52, _e150));
    let _e208 = IblConstants.environment_intensity;
    let _e214 = MaterialConstants.emissive;
    let _e216 = ((_e149 + ((((((((vec3<f32>(1f, 1f, 1f) - _e159) * (1f - _e53)) * _e54) * _e177.xyz) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + (_e193.xyz * ((_e159 * _e196.x) + vec3(_e196.y)))) * max(_e208, 0f)) * mix(1f, 1f, clamp(_e58, 0f, 1f)))) + max(_e214, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e216.x, _e216.y, _e216.z, _e45.w);
    return;
}

@fragment
fn fragment_main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec3<f32>, @location(3) in_u002e_var_u002e_TEXCOORD3_: vec3<f32>) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    in_u002e_var_u002e_TEXCOORD3_1 = in_u002e_var_u002e_TEXCOORD3_;
    fragment_main_1();
    let _e5 = out_u002e_var_u002e_SV_Target0_;
    return _e5;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
