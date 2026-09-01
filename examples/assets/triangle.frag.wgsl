var<private> fragment_color: vec4<f32>;
var<private> vertex_color_1: vec3<f32>;

fn main_1() {
    let _e3 = vertex_color_1;
    fragment_color = vec4<f32>(_e3.x, _e3.y, _e3.z, 1f);
    return;
}

@fragment
fn main(@location(0) vertex_color: vec3<f32>) -> @location(0) vec4<f32> {
    vertex_color_1 = vertex_color;
    main_1();
    let _e3 = fragment_color;
    return _e3;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
