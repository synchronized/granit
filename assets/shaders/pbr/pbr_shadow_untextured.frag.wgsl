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
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> in_u002e_var_u002e_TEXCOORD3_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_102_: bool;
    var phi_108_: bool;
    var phi_114_: bool;
    var phi_125_: f32;
    var phi_176_: vec3<f32>;

    let _e32 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e33 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e35 = MaterialConstants.base_color;
    let _e37 = MaterialConstants.metallic;
    let _e39 = MaterialConstants.perceptual_roughness;
    let _e40 = normalize(_e32);
    let _e41 = clamp(_e37, 0f, 1f);
    switch bitcast<i32>(0u) {
        default: {
            let _e44 = ShadowConstants.shadow_normal_bias;
            let _e46 = (_e33 + (_e40 * _e44));
            let _e48 = ShadowConstants.light_view_projection;
            let _e54 = (vec4<f32>(_e46.x, _e46.y, _e46.z, 1f) * transpose(_e48));
            if (_e54.w <= 0f) {
                phi_125_ = 1f;
                break;
            }
            let _e59 = (_e54.xyz / vec3(_e54.w));
            let _e62 = ((_e59.xy * vec2<f32>(0.5f, -0.5f)) + vec2<f32>(0.5f, 0.5f));
            phi_102_ = true;
            if !(any((_e62 < vec2<f32>(0f, 0f)))) {
                phi_102_ = any((_e62 > vec2<f32>(1f, 1f)));
            }
            let _e69 = phi_102_;
            phi_108_ = true;
            if !(_e69) {
                phi_108_ = (_e59.z < 0f);
            }
            let _e74 = phi_108_;
            phi_114_ = true;
            if !(_e74) {
                phi_114_ = (_e59.z > 1f);
            }
            let _e79 = phi_114_;
            if _e79 {
                phi_125_ = 1f;
                break;
            }
            let _e82 = ShadowConstants.shadow_depth_bias;
            let _e84 = textureSampleCompareLevel(shadow_texture, shadow_sampler, _e62, (_e59.z - _e82));
            phi_125_ = _e84;
            break;
        }
    }
    let _e86 = phi_125_;
    let _e87 = _e35.xyz;
    switch bitcast<i32>(0u) {
        default: {
            let _e90 = clamp(_e40.z, 0f, 1f);
            if (_e90 <= 0f) {
                phi_176_ = vec3<f32>(0f, 0f, 0f);
                break;
            }
            let _e92 = normalize(vec3<f32>(0f, 0f, 2f));
            let _e94 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e87, vec3(_e41));
            let _e102 = (_e94 + ((vec3<f32>(1f, 1f, 1f) - _e94) * pow((1f - clamp(clamp(_e92.z, 0f, 1f), 0f, 1f)), 5f)));
            let _e104 = clamp(dot(_e40, _e92), 0f, 1f);
            let _e106 = max(clamp(_e39, 0f, 1f), 0.045f);
            let _e107 = (_e106 * _e106);
            let _e108 = (_e107 * _e107);
            let _e112 = (((_e104 * _e104) * (_e108 - 1f)) + 1f);
            let _e121 = (_e90 * sqrt((((_e90 * _e90) * (1f - _e108)) + _e108)));
            phi_176_ = ((((((_e87 * (vec3<f32>(1f, 1f, 1f) - _e102)) * (1f - _e41)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e102 * (_e108 / ((3.1415927f * _e112) * _e112))) * (0.5f / (_e121 + _e121)))) * _e90) * _e86);
            break;
        }
    }
    let _e135 = phi_176_;
    let _e137 = MaterialConstants.emissive;
    let _e139 = (_e135 + max(_e137, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e35.w);
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
