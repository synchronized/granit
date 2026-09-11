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
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_100_: vec3<f32>;

    let _e22 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e24 = MaterialConstants.base_color;
    let _e26 = MaterialConstants.metallic;
    let _e28 = MaterialConstants.perceptual_roughness;
    let _e29 = normalize(_e22);
    let _e30 = clamp(_e26, 0f, 1f);
    let _e31 = _e24.xyz;
    switch bitcast<i32>(0u) {
        default: {
            let _e34 = clamp(_e29.z, 0f, 1f);
            if (_e34 <= 0f) {
                phi_100_ = vec3<f32>(0f, 0f, 0f);
                break;
            }
            let _e36 = normalize(vec3<f32>(0f, 0f, 2f));
            let _e38 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e31, vec3(_e30));
            let _e46 = (_e38 + ((vec3<f32>(1f, 1f, 1f) - _e38) * pow((1f - clamp(clamp(_e36.z, 0f, 1f), 0f, 1f)), 5f)));
            let _e48 = clamp(dot(_e29, _e36), 0f, 1f);
            let _e50 = max(clamp(_e28, 0f, 1f), 0.045f);
            let _e51 = (_e50 * _e50);
            let _e52 = (_e51 * _e51);
            let _e56 = (((_e48 * _e48) * (_e52 - 1f)) + 1f);
            let _e65 = (_e34 * sqrt((((_e34 * _e34) * (1f - _e52)) + _e52)));
            phi_100_ = ((((((_e31 * (vec3<f32>(1f, 1f, 1f) - _e46)) * (1f - _e30)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e46 * (_e52 / ((3.1415927f * _e56) * _e56))) * (0.5f / (_e65 + _e65)))) * _e34) * 1f);
            break;
        }
    }
    let _e79 = phi_100_;
    let _e81 = MaterialConstants.emissive;
    let _e83 = (_e79 + max(_e81, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e83.x, _e83.y, _e83.z, _e24.w);
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
