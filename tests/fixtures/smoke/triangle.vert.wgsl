struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec3<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f), 1f, array<f32, 1>(), array<f32, 1>());
var<private> gl_VertexIndex_1: i32;
var<private> vertex_color: vec3<f32>;

fn main_1() {
    var indexable: array<vec2<f32>, 3>;
    var indexable_1: array<vec3<f32>, 3>;

    let _e24 = gl_VertexIndex_1;
    indexable = array<vec2<f32>, 3>(vec2<f32>(0f, -0.6f), vec2<f32>(0.6f, 0.6f), vec2<f32>(-0.6f, 0.6f));
    let _e26 = indexable[_e24];
    unnamed.gl_Position = vec4<f32>(_e26.x, _e26.y, 0f, 1f);
    let _e31 = gl_VertexIndex_1;
    indexable_1 = array<vec3<f32>, 3>(vec3<f32>(1f, 0.15f, 0.1f), vec3<f32>(0.1f, 1f, 0.2f), vec3<f32>(0.15f, 0.3f, 1f));
    let _e33 = indexable_1[_e31];
    vertex_color = _e33;
    return;
}

@vertex
fn main(@builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e7 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e7);
    let _e9 = unnamed.gl_Position;
    let _e10 = vertex_color;
    return VertexOutput(_e9, _e10);
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
