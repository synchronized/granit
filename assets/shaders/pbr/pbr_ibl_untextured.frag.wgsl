struct type_u002e_IblConstants {
    environment_rotation_cos: f32,
    environment_rotation_sin: f32,
    environment_intensity: f32,
    prefiltered_environment_max_mip: f32,
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
@group(1) @binding(0)
var<uniform> MaterialConstants: type_u002e_MaterialConstants;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_125_: vec3<f32>;

    let _e32 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e34 = MaterialConstants.base_color;
    let _e36 = MaterialConstants.metallic;
    let _e38 = MaterialConstants.perceptual_roughness;
    let _e39 = normalize(_e32);
    let _e41 = clamp(_e39.z, 0f, 1f);
    let _e42 = clamp(_e36, 0f, 1f);
    let _e43 = _e34.xyz;
    let _e45 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e43, vec3(_e42));
    let _e47 = MaterialConstants.occlusion_strength;
    switch bitcast<i32>(0u) {
        default: {
            if (_e41 <= 0f) {
                phi_125_ = vec3<f32>(0f, 0f, 0f);
                break;
            }
            let _e52 = normalize(vec3<f32>(0f, 0f, 2f));
            let _e60 = (_e45 + ((vec3<f32>(1f, 1f, 1f) - _e45) * pow((1f - clamp(clamp(_e52.z, 0f, 1f), 0f, 1f)), 5f)));
            let _e62 = clamp(dot(_e39, _e52), 0f, 1f);
            let _e64 = max(clamp(_e38, 0f, 1f), 0.045f);
            let _e65 = (_e64 * _e64);
            let _e66 = (_e65 * _e65);
            let _e70 = (((_e62 * _e62) * (_e66 - 1f)) + 1f);
            let _e79 = (_e41 * sqrt((((_e41 * _e41) * (1f - _e66)) + _e66)));
            phi_125_ = ((((((_e43 * (vec3<f32>(1f, 1f, 1f) - _e60)) * (1f - _e42)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e60 * (_e66 / ((3.1415927f * _e70) * _e70))) * (0.5f / (_e79 + _e79)))) * _e41) * 1f);
            break;
        }
    }
    let _e93 = phi_125_;
    let _e94 = clamp(_e38, 0f, 1f);
    let _e103 = (_e45 + ((max(vec3((1f - _e94)), _e45) - _e45) * pow((1f - clamp(_e41, 0f, 1f)), 5f)));
    let _e108 = IblConstants.environment_rotation_cos;
    let _e112 = IblConstants.environment_rotation_sin;
    let _e116 = -(_e112);
    let _e121 = textureSample(irradiance_texture, environment_sampler, vec3<f32>(((_e108 * _e39.x) + (_e112 * _e39.z)), _e39.y, ((_e116 * _e39.x) + (_e108 * _e39.z))));
    let _e123 = reflect(vec3<f32>(-0f, -0f, -1f), _e39);
    let _e135 = IblConstants.prefiltered_environment_max_mip;
    let _e137 = textureSampleLevel(prefiltered_environment_texture, environment_sampler, vec3<f32>(((_e108 * _e123.x) + (_e112 * _e123.z)), _e123.y, ((_e116 * _e123.x) + (_e108 * _e123.z))), (_e94 * _e135));
    let _e140 = textureSample(brdf_lut_texture, environment_sampler, vec2<f32>(_e41, _e94));
    let _e152 = IblConstants.environment_intensity;
    let _e158 = MaterialConstants.emissive;
    let _e160 = ((_e93 + ((((((((vec3<f32>(1f, 1f, 1f) - _e103) * (1f - _e42)) * _e43) * _e121.xyz) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + (_e137.xyz * ((_e103 * _e140.x) + vec3(_e140.y)))) * max(_e152, 0f)) * mix(1f, 1f, clamp(_e47, 0f, 1f)))) + max(_e158, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e160.x, _e160.y, _e160.z, _e34.w);
    return;
}

@fragment
fn fragment_main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec3<f32>) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    fragment_main_1();
    let _e3 = out_u002e_var_u002e_SV_Target0_;
    return _e3;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
