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
    var phi_101_: vec3<f32>;
    var phi_104_: u32;
    var phi_146_: bool;
    var phi_152_: bool;
    var phi_158_: bool;
    var phi_169_: f32;
    var phi_170_: f32;
    var phi_236_: vec3<f32>;
    var phi_238_: vec3<f32>;
    var phi_241_: u32;
    var phi_263_: bool;
    var phi_274_: f32;
    var phi_342_: vec3<f32>;
    var phi_239_: vec3<f32>;
    var phi_345_: vec3<f32>;
    var phi_348_: u32;
    var phi_370_: bool;
    var phi_381_: f32;
    var phi_407_: f32;
    var phi_476_: vec3<f32>;
    var phi_346_: vec3<f32>;
    var local: vec3<f32>;
    var local_1: vec3<f32>;
    var local_2: vec3<f32>;
    var local_3: vec3<f32>;
    var local_4: vec3<f32>;
    var local_5: vec3<f32>;

    let _e43 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e44 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e46 = MaterialConstants.base_color;
    let _e48 = MaterialConstants.metallic;
    let _e50 = MaterialConstants.perceptual_roughness;
    let _e51 = normalize(_e43);
    let _e52 = clamp(_e48, 0f, 1f);
    phi_101_ = vec3<f32>(0f, 0f, 0f);
    phi_104_ = 0u;
    loop {
        let _e54 = phi_101_;
        let _e56 = phi_104_;
        let _e58 = LightCounts.directional_light_count;
        local_2 = _e54;
        if (_e56 < min(_e58, 16u)) {
            if (_e56 == 0u) {
                switch bitcast<i32>(0u) {
                    default: {
                        let _e64 = ShadowConstants.shadow_normal_bias;
                        let _e66 = (_e44 + (_e51 * _e64));
                        let _e68 = ShadowConstants.light_view_projection;
                        let _e74 = (vec4<f32>(_e66.x, _e66.y, _e66.z, 1f) * transpose(_e68));
                        if (_e74.w <= 0f) {
                            phi_169_ = 1f;
                            break;
                        }
                        let _e79 = (_e74.xyz / vec3(_e74.w));
                        let _e82 = ((_e79.xy * vec2<f32>(0.5f, -0.5f)) + vec2<f32>(0.5f, 0.5f));
                        phi_146_ = true;
                        if !(any((_e82 < vec2<f32>(0f, 0f)))) {
                            phi_146_ = any((_e82 > vec2<f32>(1f, 1f)));
                        }
                        let _e89 = phi_146_;
                        phi_152_ = true;
                        if !(_e89) {
                            phi_152_ = (_e79.z < 0f);
                        }
                        let _e94 = phi_152_;
                        phi_158_ = true;
                        if !(_e94) {
                            phi_158_ = (_e79.z > 1f);
                        }
                        let _e99 = phi_158_;
                        if _e99 {
                            phi_169_ = 1f;
                            break;
                        }
                        let _e102 = ShadowConstants.shadow_depth_bias;
                        let _e104 = textureSampleCompareLevel(shadow_texture, shadow_sampler, _e82, (_e79.z - _e102));
                        phi_169_ = _e104;
                        break;
                    }
                }
                let _e106 = phi_169_;
                phi_170_ = _e106;
            } else {
                phi_170_ = 1f;
            }
            let _e108 = phi_170_;
            let _e112 = directional_lights.member[_e56].direction_to_light;
            let _e113 = normalize(_e112);
            let _e117 = directional_lights.member[_e56].radiance;
            let _e119 = _e46.xyz;
            switch bitcast<i32>(0u) {
                default: {
                    let _e122 = clamp(_e51.z, 0f, 1f);
                    let _e124 = clamp(dot(_e51, _e113), 0f, 1f);
                    if (_e124 <= 0f) {
                        phi_236_ = vec3<f32>(0f, 0f, 0f);
                        break;
                    }
                    let _e127 = normalize((vec3<f32>(0f, 0f, 1f) + _e113));
                    let _e129 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e119, vec3(_e52));
                    let _e137 = (_e129 + ((vec3<f32>(1f, 1f, 1f) - _e129) * pow((1f - clamp(clamp(_e127.z, 0f, 1f), 0f, 1f)), 5f)));
                    let _e139 = clamp(dot(_e51, _e127), 0f, 1f);
                    let _e141 = max(clamp(_e50, 0f, 1f), 0.045f);
                    let _e142 = (_e141 * _e141);
                    let _e143 = (_e142 * _e142);
                    let _e147 = (((_e139 * _e139) * (_e143 - 1f)) + 1f);
                    let _e152 = (1f - _e143);
                    phi_236_ = (((((((_e119 * (vec3<f32>(1f, 1f, 1f) - _e137)) * (1f - _e52)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e137 * (_e143 / ((3.1415927f * _e147) * _e147))) * (0.5f / ((_e124 * sqrt((((_e122 * _e122) * _e152) + _e143))) + (_e122 * sqrt((((_e124 * _e124) * _e152) + _e143))))))) * _e124) * max(_e117, vec3<f32>(0f, 0f, 0f))) * _e108);
                    break;
                }
            }
            let _e176 = phi_236_;
            local_1 = (_e54 + _e176);
            continue;
        } else {
            break;
        }
        continuing {
            let _e433 = local_1;
            phi_101_ = _e433;
            phi_104_ = (_e56 + 1u);
        }
    }
    let _e443 = local_2;
    phi_238_ = _e443;
    phi_241_ = 0u;
    loop {
        let _e180 = phi_238_;
        let _e182 = phi_241_;
        let _e184 = LightCounts.point_light_count;
        local_4 = _e180;
        if (_e182 < min(_e184, 256u)) {
            let _e190 = point_lights.member[_e182].position;
            let _e191 = (_e190 - _e44);
            let _e192 = dot(_e191, _e191);
            let _e196 = point_lights.member[_e182].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_263_ = true;
                    if !((_e196 <= 0f)) {
                        phi_263_ = (_e192 >= (_e196 * _e196));
                    }
                    let _e203 = phi_263_;
                    if _e203 {
                        phi_274_ = 0f;
                        break;
                    }
                    let _e205 = (_e192 / (_e196 * _e196));
                    let _e208 = max((1f - (_e205 * _e205)), 0f);
                    phi_274_ = ((_e208 * _e208) / max(_e192, 0.0001f));
                    break;
                }
            }
            let _e213 = phi_274_;
            phi_239_ = _e180;
            if (_e213 > 0f) {
                let _e215 = normalize(_e191);
                let _e219 = point_lights.member[_e182].intensity;
                let _e222 = _e46.xyz;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e225 = clamp(_e51.z, 0f, 1f);
                        let _e227 = clamp(dot(_e51, _e215), 0f, 1f);
                        if (_e227 <= 0f) {
                            phi_342_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e230 = normalize((vec3<f32>(0f, 0f, 1f) + _e215));
                        let _e232 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e222, vec3(_e52));
                        let _e240 = (_e232 + ((vec3<f32>(1f, 1f, 1f) - _e232) * pow((1f - clamp(clamp(_e230.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e242 = clamp(dot(_e51, _e230), 0f, 1f);
                        let _e244 = max(clamp(_e50, 0f, 1f), 0.045f);
                        let _e245 = (_e244 * _e244);
                        let _e246 = (_e245 * _e245);
                        let _e250 = (((_e242 * _e242) * (_e246 - 1f)) + 1f);
                        let _e255 = (1f - _e246);
                        phi_342_ = (((((((_e222 * (vec3<f32>(1f, 1f, 1f) - _e240)) * (1f - _e52)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e240 * (_e246 / ((3.1415927f * _e250) * _e250))) * (0.5f / ((_e227 * sqrt((((_e225 * _e225) * _e255) + _e246))) + (_e225 * sqrt((((_e227 * _e227) * _e255) + _e246))))))) * _e227) * (max(_e219, vec3<f32>(0f, 0f, 0f)) * _e213)) * 1f);
                        break;
                    }
                }
                let _e279 = phi_342_;
                phi_239_ = (_e180 + _e279);
            }
            let _e282 = phi_239_;
            local_3 = _e282;
            continue;
        } else {
            break;
        }
        continuing {
            let _e445 = local_3;
            phi_238_ = _e445;
            phi_241_ = (_e182 + 1u);
        }
    }
    let _e453 = local_4;
    phi_345_ = _e453;
    phi_348_ = 0u;
    loop {
        let _e285 = phi_345_;
        let _e287 = phi_348_;
        let _e289 = LightCounts.spot_light_count;
        local = _e285;
        if (_e287 < min(_e289, 256u)) {
            let _e295 = spot_lights.member[_e287].position;
            let _e296 = (_e295 - _e44);
            let _e297 = dot(_e296, _e296);
            let _e301 = spot_lights.member[_e287].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_370_ = true;
                    if !((_e301 <= 0f)) {
                        phi_370_ = (_e297 >= (_e301 * _e301));
                    }
                    let _e308 = phi_370_;
                    if _e308 {
                        phi_381_ = 0f;
                        break;
                    }
                    let _e310 = (_e297 / (_e301 * _e301));
                    let _e313 = max((1f - (_e310 * _e310)), 0f);
                    phi_381_ = ((_e313 * _e313) / max(_e297, 0.0001f));
                    break;
                }
            }
            let _e318 = phi_381_;
            let _e323 = spot_lights.member[_e287].direction;
            let _e327 = spot_lights.member[_e287].inner_angle_cosine;
            let _e331 = spot_lights.member[_e287].outer_angle_cosine;
            switch bitcast<i32>(0u) {
                default: {
                    let _e335 = dot(normalize(-(_e296)), normalize(_e323));
                    let _e336 = (_e327 - _e331);
                    if (_e336 <= 0f) {
                        phi_407_ = select(0f, 1f, (_e335 >= _e331));
                        break;
                    }
                    let _e342 = clamp(((_e335 - _e331) / _e336), 0f, 1f);
                    phi_407_ = ((_e342 * _e342) * (3f - (2f * _e342)));
                    break;
                }
            }
            let _e348 = phi_407_;
            let _e349 = (_e318 * _e348);
            phi_346_ = _e285;
            if (_e349 > 0f) {
                let _e351 = normalize(_e296);
                let _e355 = spot_lights.member[_e287].intensity;
                let _e358 = _e46.xyz;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e361 = clamp(_e51.z, 0f, 1f);
                        let _e363 = clamp(dot(_e51, _e351), 0f, 1f);
                        if (_e363 <= 0f) {
                            phi_476_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e366 = normalize((vec3<f32>(0f, 0f, 1f) + _e351));
                        let _e368 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e358, vec3(_e52));
                        let _e376 = (_e368 + ((vec3<f32>(1f, 1f, 1f) - _e368) * pow((1f - clamp(clamp(_e366.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e378 = clamp(dot(_e51, _e366), 0f, 1f);
                        let _e380 = max(clamp(_e50, 0f, 1f), 0.045f);
                        let _e381 = (_e380 * _e380);
                        let _e382 = (_e381 * _e381);
                        let _e386 = (((_e378 * _e378) * (_e382 - 1f)) + 1f);
                        let _e391 = (1f - _e382);
                        phi_476_ = (((((((_e358 * (vec3<f32>(1f, 1f, 1f) - _e376)) * (1f - _e52)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e376 * (_e382 / ((3.1415927f * _e386) * _e386))) * (0.5f / ((_e363 * sqrt((((_e361 * _e361) * _e391) + _e382))) + (_e361 * sqrt((((_e363 * _e363) * _e391) + _e382))))))) * _e363) * (max(_e355, vec3<f32>(0f, 0f, 0f)) * _e349)) * 1f);
                        break;
                    }
                }
                let _e415 = phi_476_;
                phi_346_ = (_e285 + _e415);
            }
            let _e418 = phi_346_;
            local_5 = _e418;
            continue;
        } else {
            break;
        }
        continuing {
            let _e455 = local_5;
            phi_345_ = _e455;
            phi_348_ = (_e287 + 1u);
        }
    }
    let _e421 = MaterialConstants.emissive;
    let _e424 = local;
    let _e425 = (_e424 + max(_e421, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e425.x, _e425.y, _e425.z, _e46.w);
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
