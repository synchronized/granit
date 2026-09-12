struct type_u002e_LightCounts {
    directional_light_count: u32,
    point_light_count: u32,
    spot_light_count: u32,
    light_count_padding: u32,
}

struct directional_light_data {
    direction_to_light: vec3<f32>,
    padding0_: f32,
    radiance: vec3<f32>,
    padding1_: f32,
}

struct type_u002e_StructuredBuffer_u002e_directional_light_data {
    member: array<directional_light_data>,
}

struct point_light_data {
    position: vec3<f32>,
    radius: f32,
    intensity: vec3<f32>,
    padding: f32,
}

struct type_u002e_StructuredBuffer_u002e_point_light_data {
    member: array<point_light_data>,
}

struct spot_light_data {
    position: vec3<f32>,
    radius: f32,
    direction: vec3<f32>,
    outer_angle_cosine: f32,
    intensity: vec3<f32>,
    inner_angle_cosine: f32,
}

struct type_u002e_StructuredBuffer_u002e_spot_light_data {
    member: array<spot_light_data>,
}

struct type_u002e_IblConstants {
    environment_rotation_cos: f32,
    environment_rotation_sin: f32,
    environment_intensity: f32,
    prefiltered_environment_max_mip: f32,
}

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

@group(3) @binding(8)
var<uniform> LightCounts: type_u002e_LightCounts;
@group(3) @binding(9)
var<storage, read_write> directional_lights: type_u002e_StructuredBuffer_u002e_directional_light_data;
@group(3) @binding(10)
var<storage, read_write> point_lights: type_u002e_StructuredBuffer_u002e_point_light_data;
@group(3) @binding(11)
var<storage, read_write> spot_lights: type_u002e_StructuredBuffer_u002e_spot_light_data;
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
    var phi_123_: vec3<f32>;
    var phi_126_: u32;
    var phi_168_: bool;
    var phi_174_: bool;
    var phi_180_: bool;
    var phi_191_: f32;
    var phi_192_: f32;
    var phi_253_: vec3<f32>;
    var phi_255_: vec3<f32>;
    var phi_258_: u32;
    var phi_280_: bool;
    var phi_291_: f32;
    var phi_354_: vec3<f32>;
    var phi_256_: vec3<f32>;
    var phi_357_: vec3<f32>;
    var phi_360_: u32;
    var phi_382_: bool;
    var phi_393_: f32;
    var phi_419_: f32;
    var phi_483_: vec3<f32>;
    var phi_358_: vec3<f32>;
    var local: vec3<f32>;
    var local_1: vec3<f32>;
    var local_2: vec3<f32>;
    var local_3: vec3<f32>;
    var local_4: vec3<f32>;
    var local_5: vec3<f32>;

    let _e51 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e52 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e54 = MaterialConstants.base_color;
    let _e56 = MaterialConstants.metallic;
    let _e58 = MaterialConstants.perceptual_roughness;
    let _e59 = normalize(_e51);
    let _e61 = clamp(_e59.z, 0f, 1f);
    let _e62 = clamp(_e56, 0f, 1f);
    let _e63 = _e54.xyz;
    let _e65 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e63, vec3(_e62));
    let _e67 = MaterialConstants.occlusion_strength;
    phi_123_ = vec3<f32>(0f, 0f, 0f);
    phi_126_ = 0u;
    loop {
        let _e71 = phi_123_;
        let _e73 = phi_126_;
        let _e75 = LightCounts.directional_light_count;
        local_2 = _e71;
        if (_e73 < min(_e75, 16u)) {
            if (_e73 == 0u) {
                switch bitcast<i32>(0u) {
                    default: {
                        let _e81 = ShadowConstants.shadow_normal_bias;
                        let _e83 = (_e52 + (_e59 * _e81));
                        let _e85 = ShadowConstants.light_view_projection;
                        let _e91 = (vec4<f32>(_e83.x, _e83.y, _e83.z, 1f) * transpose(_e85));
                        if (_e91.w <= 0f) {
                            phi_191_ = 1f;
                            break;
                        }
                        let _e96 = (_e91.xyz / vec3(_e91.w));
                        let _e99 = ((_e96.xy * vec2<f32>(0.5f, -0.5f)) + vec2<f32>(0.5f, 0.5f));
                        phi_168_ = true;
                        if !(any((_e99 < vec2<f32>(0f, 0f)))) {
                            phi_168_ = any((_e99 > vec2<f32>(1f, 1f)));
                        }
                        let _e106 = phi_168_;
                        phi_174_ = true;
                        if !(_e106) {
                            phi_174_ = (_e96.z < 0f);
                        }
                        let _e111 = phi_174_;
                        phi_180_ = true;
                        if !(_e111) {
                            phi_180_ = (_e96.z > 1f);
                        }
                        let _e116 = phi_180_;
                        if _e116 {
                            phi_191_ = 1f;
                            break;
                        }
                        let _e119 = ShadowConstants.shadow_depth_bias;
                        let _e121 = textureSampleCompareLevel(shadow_texture, shadow_sampler, _e99, (_e96.z - _e119));
                        phi_191_ = _e121;
                        break;
                    }
                }
                let _e123 = phi_191_;
                phi_192_ = _e123;
            } else {
                phi_192_ = 1f;
            }
            let _e125 = phi_192_;
            let _e129 = directional_lights.member[_e73].direction_to_light;
            let _e130 = normalize(_e129);
            let _e134 = directional_lights.member[_e73].radiance;
            switch bitcast<i32>(0u) {
                default: {
                    let _e138 = clamp(dot(_e59, _e130), 0f, 1f);
                    if (_e138 <= 0f) {
                        phi_253_ = vec3<f32>(0f, 0f, 0f);
                        break;
                    }
                    let _e141 = normalize((vec3<f32>(0f, 0f, 1f) + _e130));
                    let _e149 = (_e65 + ((vec3<f32>(1f, 1f, 1f) - _e65) * pow((1f - clamp(clamp(_e141.z, 0f, 1f), 0f, 1f)), 5f)));
                    let _e151 = clamp(dot(_e59, _e141), 0f, 1f);
                    let _e153 = max(clamp(_e58, 0f, 1f), 0.045f);
                    let _e154 = (_e153 * _e153);
                    let _e155 = (_e154 * _e154);
                    let _e159 = (((_e151 * _e151) * (_e155 - 1f)) + 1f);
                    let _e164 = (1f - _e155);
                    phi_253_ = (((((((_e63 * (vec3<f32>(1f, 1f, 1f) - _e149)) * (1f - _e62)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e149 * (_e155 / ((3.1415927f * _e159) * _e159))) * (0.5f / ((_e138 * sqrt((((_e61 * _e61) * _e164) + _e155))) + (_e61 * sqrt((((_e138 * _e138) * _e164) + _e155))))))) * _e138) * max(_e134, vec3<f32>(0f, 0f, 0f))) * _e125);
                    break;
                }
            }
            let _e188 = phi_253_;
            local_1 = (_e71 + _e188);
            continue;
        } else {
            break;
        }
        continuing {
            let _e498 = local_1;
            phi_123_ = _e498;
            phi_126_ = (_e73 + 1u);
        }
    }
    let _e508 = local_2;
    phi_255_ = _e508;
    phi_258_ = 0u;
    loop {
        let _e192 = phi_255_;
        let _e194 = phi_258_;
        let _e196 = LightCounts.point_light_count;
        local_4 = _e192;
        if (_e194 < min(_e196, 256u)) {
            let _e202 = point_lights.member[_e194].position;
            let _e203 = (_e202 - _e52);
            let _e204 = dot(_e203, _e203);
            let _e208 = point_lights.member[_e194].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_280_ = true;
                    if !((_e208 <= 0f)) {
                        phi_280_ = (_e204 >= (_e208 * _e208));
                    }
                    let _e215 = phi_280_;
                    if _e215 {
                        phi_291_ = 0f;
                        break;
                    }
                    let _e217 = (_e204 / (_e208 * _e208));
                    let _e220 = max((1f - (_e217 * _e217)), 0f);
                    phi_291_ = ((_e220 * _e220) / max(_e204, 0.0001f));
                    break;
                }
            }
            let _e225 = phi_291_;
            phi_256_ = _e192;
            if (_e225 > 0f) {
                let _e227 = normalize(_e203);
                let _e231 = point_lights.member[_e194].intensity;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e236 = clamp(dot(_e59, _e227), 0f, 1f);
                        if (_e236 <= 0f) {
                            phi_354_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e239 = normalize((vec3<f32>(0f, 0f, 1f) + _e227));
                        let _e247 = (_e65 + ((vec3<f32>(1f, 1f, 1f) - _e65) * pow((1f - clamp(clamp(_e239.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e249 = clamp(dot(_e59, _e239), 0f, 1f);
                        let _e251 = max(clamp(_e58, 0f, 1f), 0.045f);
                        let _e252 = (_e251 * _e251);
                        let _e253 = (_e252 * _e252);
                        let _e257 = (((_e249 * _e249) * (_e253 - 1f)) + 1f);
                        let _e262 = (1f - _e253);
                        phi_354_ = (((((((_e63 * (vec3<f32>(1f, 1f, 1f) - _e247)) * (1f - _e62)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e247 * (_e253 / ((3.1415927f * _e257) * _e257))) * (0.5f / ((_e236 * sqrt((((_e61 * _e61) * _e262) + _e253))) + (_e61 * sqrt((((_e236 * _e236) * _e262) + _e253))))))) * _e236) * (max(_e231, vec3<f32>(0f, 0f, 0f)) * _e225)) * 1f);
                        break;
                    }
                }
                let _e286 = phi_354_;
                phi_256_ = (_e192 + _e286);
            }
            let _e289 = phi_256_;
            local_3 = _e289;
            continue;
        } else {
            break;
        }
        continuing {
            let _e510 = local_3;
            phi_255_ = _e510;
            phi_258_ = (_e194 + 1u);
        }
    }
    let _e518 = local_4;
    phi_357_ = _e518;
    phi_360_ = 0u;
    loop {
        let _e292 = phi_357_;
        let _e294 = phi_360_;
        let _e296 = LightCounts.spot_light_count;
        local = _e292;
        if (_e294 < min(_e296, 256u)) {
            let _e302 = spot_lights.member[_e294].position;
            let _e303 = (_e302 - _e52);
            let _e304 = dot(_e303, _e303);
            let _e308 = spot_lights.member[_e294].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_382_ = true;
                    if !((_e308 <= 0f)) {
                        phi_382_ = (_e304 >= (_e308 * _e308));
                    }
                    let _e315 = phi_382_;
                    if _e315 {
                        phi_393_ = 0f;
                        break;
                    }
                    let _e317 = (_e304 / (_e308 * _e308));
                    let _e320 = max((1f - (_e317 * _e317)), 0f);
                    phi_393_ = ((_e320 * _e320) / max(_e304, 0.0001f));
                    break;
                }
            }
            let _e325 = phi_393_;
            let _e330 = spot_lights.member[_e294].direction;
            let _e334 = spot_lights.member[_e294].inner_angle_cosine;
            let _e338 = spot_lights.member[_e294].outer_angle_cosine;
            switch bitcast<i32>(0u) {
                default: {
                    let _e342 = dot(normalize(-(_e303)), normalize(_e330));
                    let _e343 = (_e334 - _e338);
                    if (_e343 <= 0f) {
                        phi_419_ = select(0f, 1f, (_e342 >= _e338));
                        break;
                    }
                    let _e349 = clamp(((_e342 - _e338) / _e343), 0f, 1f);
                    phi_419_ = ((_e349 * _e349) * (3f - (2f * _e349)));
                    break;
                }
            }
            let _e355 = phi_419_;
            let _e356 = (_e325 * _e355);
            phi_358_ = _e292;
            if (_e356 > 0f) {
                let _e358 = normalize(_e303);
                let _e362 = spot_lights.member[_e294].intensity;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e367 = clamp(dot(_e59, _e358), 0f, 1f);
                        if (_e367 <= 0f) {
                            phi_483_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e370 = normalize((vec3<f32>(0f, 0f, 1f) + _e358));
                        let _e378 = (_e65 + ((vec3<f32>(1f, 1f, 1f) - _e65) * pow((1f - clamp(clamp(_e370.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e380 = clamp(dot(_e59, _e370), 0f, 1f);
                        let _e382 = max(clamp(_e58, 0f, 1f), 0.045f);
                        let _e383 = (_e382 * _e382);
                        let _e384 = (_e383 * _e383);
                        let _e388 = (((_e380 * _e380) * (_e384 - 1f)) + 1f);
                        let _e393 = (1f - _e384);
                        phi_483_ = (((((((_e63 * (vec3<f32>(1f, 1f, 1f) - _e378)) * (1f - _e62)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e378 * (_e384 / ((3.1415927f * _e388) * _e388))) * (0.5f / ((_e367 * sqrt((((_e61 * _e61) * _e393) + _e384))) + (_e61 * sqrt((((_e367 * _e367) * _e393) + _e384))))))) * _e367) * (max(_e362, vec3<f32>(0f, 0f, 0f)) * _e356)) * 1f);
                        break;
                    }
                }
                let _e417 = phi_483_;
                phi_358_ = (_e292 + _e417);
            }
            let _e420 = phi_358_;
            local_5 = _e420;
            continue;
        } else {
            break;
        }
        continuing {
            let _e520 = local_5;
            phi_357_ = _e520;
            phi_360_ = (_e294 + 1u);
        }
    }
    let _e422 = clamp(_e58, 0f, 1f);
    let _e431 = (_e65 + ((max(vec3((1f - _e422)), _e65) - _e65) * pow((1f - clamp(_e61, 0f, 1f)), 5f)));
    let _e436 = IblConstants.environment_rotation_cos;
    let _e440 = IblConstants.environment_rotation_sin;
    let _e444 = -(_e440);
    let _e449 = textureSample(irradiance_texture, environment_sampler, vec3<f32>(((_e436 * _e59.x) + (_e440 * _e59.z)), _e59.y, ((_e444 * _e59.x) + (_e436 * _e59.z))));
    let _e451 = reflect(vec3<f32>(-0f, -0f, -1f), _e59);
    let _e463 = IblConstants.prefiltered_environment_max_mip;
    let _e465 = textureSampleLevel(prefiltered_environment_texture, environment_sampler, vec3<f32>(((_e436 * _e451.x) + (_e440 * _e451.z)), _e451.y, ((_e444 * _e451.x) + (_e436 * _e451.z))), (_e422 * _e463));
    let _e468 = textureSample(brdf_lut_texture, environment_sampler, vec2<f32>(_e61, _e422));
    let _e480 = IblConstants.environment_intensity;
    let _e485 = local;
    let _e488 = MaterialConstants.emissive;
    let _e490 = ((_e485 + ((((((((vec3<f32>(1f, 1f, 1f) - _e431) * (1f - _e62)) * _e63) * _e449.xyz) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + (_e465.xyz * ((_e431 * _e468.x) + vec3(_e468.y)))) * max(_e480, 0f)) * mix(1f, 1f, clamp(_e67, 0f, 1f)))) + max(_e488, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e490.x, _e490.y, _e490.z, _e54.w);
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
