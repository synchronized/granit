var<private> in_u002e_var_u002e_COLOR0_1: vec4<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    let _e14 = in_u002e_var_u002e_COLOR0_1;
    let _e15 = _e14.xyz;
    let _e23 = mix(((pow(max(_e15, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.41666666f, 0.41666666f, 0.41666666f)) * 1.055f) - vec3<f32>(0.055f, 0.055f, 0.055f)), (_e15 * 12.92f), select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), (_e15 <= vec3<f32>(0.0031308f, 0.0031308f, 0.0031308f))));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e23.x, _e23.y, _e23.z, _e14.w);
    return;
}

@fragment
fn fragment_main(@location(0) in_u002e_var_u002e_COLOR0_: vec4<f32>) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_COLOR0_1 = in_u002e_var_u002e_COLOR0_;
    fragment_main_1();
    let _e3 = out_u002e_var_u002e_SV_Target0_;
    return _e3;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
