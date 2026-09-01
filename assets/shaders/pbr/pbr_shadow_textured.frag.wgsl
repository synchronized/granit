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

@group(3) @binding(0)
var<uniform> ShadowConstants: type_u002e_ShadowConstants;
@group(3) @binding(1)
var shadow_texture: texture_depth_2d;
@group(3) @binding(2)
var shadow_sampler: sampler_comparison;
@group(1) @binding(0)
var<uniform> MaterialConstants: type_u002e_MaterialConstants;
@group(1) @binding(1)
var base_color_texture: texture_2d<f32>;
@group(1) @binding(2)
var metallic_roughness_texture: texture_2d<f32>;
@group(1) @binding(3)
var normal_texture: texture_2d<f32>;
@group(1) @binding(5)
var emissive_texture: texture_2d<f32>;
@group(1) @binding(6)
var pbr_sampler: sampler;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> in_u002e_var_u002e_TEXCOORD1_1: vec4<f32>;
var<private> in_u002e_var_u002e_TEXCOORD2_1: vec2<f32>;
var<private> in_u002e_var_u002e_TEXCOORD3_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_158_: bool;
    var phi_164_: bool;
    var phi_170_: bool;
    var phi_181_: f32;
    var phi_232_: vec3<f32>;

    let _e40 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e41 = in_u002e_var_u002e_TEXCOORD1_1;
    let _e42 = in_u002e_var_u002e_TEXCOORD2_1;
    let _e43 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e45 = MaterialConstants.base_color;
    let _e47 = MaterialConstants.metallic;
    let _e49 = MaterialConstants.perceptual_roughness;
    let _e50 = textureSample(base_color_texture, pbr_sampler, _e42);
    let _e51 = (_e45 * _e50);
    let _e52 = textureSample(metallic_roughness_texture, pbr_sampler, _e42);
    let _e57 = textureSample(emissive_texture, pbr_sampler, _e42);
    let _e59 = normalize(_e40);
    let _e60 = textureSample(normal_texture, pbr_sampler, _e42);
    let _e63 = ((_e60.xyz * 2f) - vec3<f32>(1f, 1f, 1f));
    let _e65 = normalize(_e41.xyz);
    let _e73 = MaterialConstants.normal_scale;
    let _e82 = normalize(((((_e65 * _e63.x) * _e73) + (((normalize(cross(_e59, _e65)) * _e41.w) * _e63.y) * _e73)) + (_e59 * _e63.z)));
    let _e83 = clamp((_e47 * _e52.z), 0f, 1f);
    switch bitcast<i32>(0u) {
        default: {
            let _e86 = ShadowConstants.shadow_normal_bias;
            let _e88 = (_e43 + (_e82 * _e86));
            let _e90 = ShadowConstants.light_view_projection;
            let _e96 = (vec4<f32>(_e88.x, _e88.y, _e88.z, 1f) * transpose(_e90));
            if (_e96.w <= 0f) {
                phi_181_ = 1f;
                break;
            }
            let _e101 = (_e96.xyz / vec3(_e96.w));
            let _e104 = ((_e101.xy * vec2<f32>(0.5f, -0.5f)) + vec2<f32>(0.5f, 0.5f));
            phi_158_ = true;
            if !(any((_e104 < vec2<f32>(0f, 0f)))) {
                phi_158_ = any((_e104 > vec2<f32>(1f, 1f)));
            }
            let _e111 = phi_158_;
            phi_164_ = true;
            if !(_e111) {
                phi_164_ = (_e101.z < 0f);
            }
            let _e116 = phi_164_;
            phi_170_ = true;
            if !(_e116) {
                phi_170_ = (_e101.z > 1f);
            }
            let _e121 = phi_170_;
            if _e121 {
                phi_181_ = 1f;
                break;
            }
            let _e124 = ShadowConstants.shadow_depth_bias;
            let _e126 = textureSampleCompareLevel(shadow_texture, shadow_sampler, _e104, (_e101.z - _e124));
            phi_181_ = _e126;
            break;
        }
    }
    let _e128 = phi_181_;
    let _e129 = _e51.xyz;
    switch bitcast<i32>(0u) {
        default: {
            let _e132 = clamp(_e82.z, 0f, 1f);
            if (_e132 <= 0f) {
                phi_232_ = vec3<f32>(0f, 0f, 0f);
                break;
            }
            let _e134 = normalize(vec3<f32>(0f, 0f, 2f));
            let _e136 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e129, vec3(_e83));
            let _e144 = (_e136 + ((vec3<f32>(1f, 1f, 1f) - _e136) * pow((1f - clamp(clamp(_e134.z, 0f, 1f), 0f, 1f)), 5f)));
            let _e146 = clamp(dot(_e82, _e134), 0f, 1f);
            let _e148 = max(clamp((_e49 * _e52.y), 0f, 1f), 0.045f);
            let _e149 = (_e148 * _e148);
            let _e150 = (_e149 * _e149);
            let _e154 = (((_e146 * _e146) * (_e150 - 1f)) + 1f);
            let _e163 = (_e132 * sqrt((((_e132 * _e132) * (1f - _e150)) + _e150)));
            phi_232_ = ((((((_e129 * (vec3<f32>(1f, 1f, 1f) - _e144)) * (1f - _e83)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e144 * (_e150 / ((3.1415927f * _e154) * _e154))) * (0.5f / (_e163 + _e163)))) * _e132) * _e128);
            break;
        }
    }
    let _e177 = phi_232_;
    let _e179 = MaterialConstants.emissive;
    let _e182 = (_e177 + (max(_e179, vec3<f32>(0f, 0f, 0f)) * _e57.xyz));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e182.x, _e182.y, _e182.z, _e51.w);
    return;
}

@fragment
fn fragment_main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec3<f32>, @location(1) in_u002e_var_u002e_TEXCOORD1_: vec4<f32>, @location(2) in_u002e_var_u002e_TEXCOORD2_: vec2<f32>, @location(3) in_u002e_var_u002e_TEXCOORD3_: vec3<f32>) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    in_u002e_var_u002e_TEXCOORD1_1 = in_u002e_var_u002e_TEXCOORD1_;
    in_u002e_var_u002e_TEXCOORD2_1 = in_u002e_var_u002e_TEXCOORD2_;
    in_u002e_var_u002e_TEXCOORD3_1 = in_u002e_var_u002e_TEXCOORD3_;
    fragment_main_1();
    let _e9 = out_u002e_var_u002e_SV_Target0_;
    return _e9;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
