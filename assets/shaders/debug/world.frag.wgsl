var<private> in_u002e_var_u002e_COLOR0_1: vec4<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    let _e2 = in_u002e_var_u002e_COLOR0_1;
    out_u002e_var_u002e_SV_Target0_ = _e2;
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
