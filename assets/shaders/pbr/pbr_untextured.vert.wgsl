struct VertexOutput {
    @builtin(position) member: vec4<f32>,
    @location(0) member_1: vec3<f32>,
    @location(1) member_2: vec4<f32>,
    @location(2) member_3: vec2<f32>,
}

var<private> global: u32;
var<private> global_1: vec4<f32> = vec4<f32>(0f, 0f, 0f, 1f);
var<private> out_u002e_var_u002e_TEXCOORD0_: vec3<f32>;
var<private> out_u002e_var_u002e_TEXCOORD1_: vec4<f32>;
var<private> out_u002e_var_u002e_TEXCOORD2_: vec2<f32>;

fn vertex_main_1() {
    var local: array<vec2<f32>, 3>;

    let _e19 = global;
    local = array<vec2<f32>, 3>(vec2<f32>(0f, -0.65f), vec2<f32>(0.65f, 0.65f), vec2<f32>(-0.65f, 0.65f));
    let _e21 = local[_e19];
    let _e25 = local[_e19];
    global_1 = vec4<f32>(_e21.x, _e21.y, 0.5f, 1f);
    out_u002e_var_u002e_TEXCOORD0_ = vec3<f32>(0f, 0f, 1f);
    out_u002e_var_u002e_TEXCOORD1_ = vec4<f32>(1f, 0f, 0f, 1f);
    out_u002e_var_u002e_TEXCOORD2_ = ((_e25 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    return;
}

@vertex
fn vertex_main(@builtin(vertex_index) param: u32) -> VertexOutput {
    global = param;
    vertex_main_1();
    let _e7 = global_1.y;
    global_1.y = -(_e7);
    let _e9 = global_1;
    let _e10 = out_u002e_var_u002e_TEXCOORD0_;
    let _e11 = out_u002e_var_u002e_TEXCOORD1_;
    let _e12 = out_u002e_var_u002e_TEXCOORD2_;
    return VertexOutput(_e9, _e10, _e11, _e12);
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
