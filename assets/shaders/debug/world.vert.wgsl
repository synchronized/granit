struct VertexOutput {
    @builtin(position) member: vec4<f32>,
    @location(0) member_1: vec4<f32>,
}

var<private> in_u002e_var_u002e_POSITION_1: vec4<f32>;
var<private> in_u002e_var_u002e_COLOR0_1: u32;
var<private> global: vec4<f32> = vec4<f32>(0f, 0f, 0f, 1f);
var<private> out_u002e_var_u002e_COLOR0_: vec4<f32>;

fn vertex_main_1() {
    let _e10 = in_u002e_var_u002e_POSITION_1;
    let _e11 = in_u002e_var_u002e_COLOR0_1;
    global = _e10;
    out_u002e_var_u002e_COLOR0_ = (vec4<f32>(f32((_e11 & 255u)), f32(((_e11 >> bitcast<u32>(8u)) & 255u)), f32(((_e11 >> bitcast<u32>(16u)) & 255u)), f32(((_e11 >> bitcast<u32>(24u)) & 255u))) * vec4<f32>(0.003921569f, 0.003921569f, 0.003921569f, 0.003921569f));
    return;
}

@vertex
fn vertex_main(@location(0) in_u002e_var_u002e_POSITION: vec4<f32>, @location(1) in_u002e_var_u002e_COLOR0_: u32) -> VertexOutput {
    in_u002e_var_u002e_POSITION_1 = in_u002e_var_u002e_POSITION;
    in_u002e_var_u002e_COLOR0_1 = in_u002e_var_u002e_COLOR0_;
    vertex_main_1();
    let _e7 = global.y;
    global.y = -(_e7);
    let _e9 = global;
    let _e10 = out_u002e_var_u002e_COLOR0_;
    return VertexOutput(_e9, _e10);
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
