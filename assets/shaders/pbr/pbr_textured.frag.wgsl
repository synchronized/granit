struct type_u002e_MaterialConstants {
    base_color: vec4<f32>,
    metallic: f32,
    perceptual_roughness: f32,
    normal_scale: f32,
    occlusion_strength: f32,
    emissive: vec3<f32>,
    reserved_value: f32,
}

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
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_162_: vec3<f32>;

    let _e30 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e31 = in_u002e_var_u002e_TEXCOORD1_1;
    let _e32 = in_u002e_var_u002e_TEXCOORD2_1;
    let _e34 = MaterialConstants.base_color;
    let _e36 = MaterialConstants.metallic;
    let _e38 = MaterialConstants.perceptual_roughness;
    let _e39 = textureSample(base_color_texture, pbr_sampler, _e32);
    let _e40 = (_e34 * _e39);
    let _e41 = textureSample(metallic_roughness_texture, pbr_sampler, _e32);
    let _e46 = textureSample(emissive_texture, pbr_sampler, _e32);
    let _e48 = normalize(_e30);
    let _e49 = textureSample(normal_texture, pbr_sampler, _e32);
    let _e52 = ((_e49.xyz * 2f) - vec3<f32>(1f, 1f, 1f));
    let _e54 = normalize(_e31.xyz);
    let _e62 = MaterialConstants.normal_scale;
    let _e71 = normalize(((((_e54 * _e52.x) * _e62) + (((normalize(cross(_e48, _e54)) * _e31.w) * _e52.y) * _e62)) + (_e48 * _e52.z)));
    let _e72 = clamp((_e36 * _e41.z), 0f, 1f);
    let _e73 = _e40.xyz;
    switch bitcast<i32>(0u) {
        default: {
            let _e76 = clamp(_e71.z, 0f, 1f);
            if (_e76 <= 0f) {
                phi_162_ = vec3<f32>(0f, 0f, 0f);
                break;
            }
            let _e78 = normalize(vec3<f32>(0f, 0f, 2f));
            let _e80 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e73, vec3(_e72));
            let _e88 = (_e80 + ((vec3<f32>(1f, 1f, 1f) - _e80) * pow((1f - clamp(clamp(_e78.z, 0f, 1f), 0f, 1f)), 5f)));
            let _e90 = clamp(dot(_e71, _e78), 0f, 1f);
            let _e92 = max(clamp((_e38 * _e41.y), 0f, 1f), 0.045f);
            let _e93 = (_e92 * _e92);
            let _e94 = (_e93 * _e93);
            let _e98 = (((_e90 * _e90) * (_e94 - 1f)) + 1f);
            let _e107 = (_e76 * sqrt((((_e76 * _e76) * (1f - _e94)) + _e94)));
            phi_162_ = ((((((_e73 * (vec3<f32>(1f, 1f, 1f) - _e88)) * (1f - _e72)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e88 * (_e94 / ((3.1415927f * _e98) * _e98))) * (0.5f / (_e107 + _e107)))) * _e76) * 1f);
            break;
        }
    }
    let _e121 = phi_162_;
    let _e123 = MaterialConstants.emissive;
    let _e126 = (_e121 + (max(_e123, vec3<f32>(0f, 0f, 0f)) * _e46.xyz));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e126.x, _e126.y, _e126.z, _e40.w);
    return;
}

@fragment
fn fragment_main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec3<f32>, @location(1) in_u002e_var_u002e_TEXCOORD1_: vec4<f32>, @location(2) in_u002e_var_u002e_TEXCOORD2_: vec2<f32>) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    in_u002e_var_u002e_TEXCOORD1_1 = in_u002e_var_u002e_TEXCOORD1_;
    in_u002e_var_u002e_TEXCOORD2_1 = in_u002e_var_u002e_TEXCOORD2_;
    fragment_main_1();
    let _e7 = out_u002e_var_u002e_SV_Target0_;
    return _e7;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
