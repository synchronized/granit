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
@group(1) @binding(0)
var<uniform> MaterialConstants: type_u002e_MaterialConstants;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> in_u002e_var_u002e_TEXCOORD3_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_82_: vec3<f32>;
    var phi_85_: u32;
    var phi_158_: vec3<f32>;
    var phi_160_: vec3<f32>;
    var phi_163_: u32;
    var phi_185_: bool;
    var phi_196_: f32;
    var phi_264_: vec3<f32>;
    var phi_161_: vec3<f32>;
    var phi_267_: vec3<f32>;
    var phi_270_: u32;
    var phi_292_: bool;
    var phi_303_: f32;
    var phi_329_: f32;
    var phi_398_: vec3<f32>;
    var phi_268_: vec3<f32>;
    var local: vec3<f32>;
    var local_1: vec3<f32>;
    var local_2: vec3<f32>;
    var local_3: vec3<f32>;
    var local_4: vec3<f32>;
    var local_5: vec3<f32>;

    let _e35 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e36 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e38 = MaterialConstants.base_color;
    let _e40 = MaterialConstants.metallic;
    let _e42 = MaterialConstants.perceptual_roughness;
    let _e43 = normalize(_e35);
    let _e44 = clamp(_e40, 0f, 1f);
    phi_82_ = vec3<f32>(0f, 0f, 0f);
    phi_85_ = 0u;
    loop {
        let _e46 = phi_82_;
        let _e48 = phi_85_;
        let _e50 = LightCounts.directional_light_count;
        local_2 = _e46;
        if (_e48 < min(_e50, 16u)) {
            let _e56 = directional_lights.member[_e48].direction_to_light;
            let _e57 = normalize(_e56);
            let _e61 = directional_lights.member[_e48].radiance;
            let _e63 = _e38.xyz;
            switch bitcast<i32>(0u) {
                default: {
                    let _e66 = clamp(_e43.z, 0f, 1f);
                    let _e68 = clamp(dot(_e43, _e57), 0f, 1f);
                    if (_e68 <= 0f) {
                        phi_158_ = vec3<f32>(0f, 0f, 0f);
                        break;
                    }
                    let _e71 = normalize((vec3<f32>(0f, 0f, 1f) + _e57));
                    let _e73 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e63, vec3(_e44));
                    let _e81 = (_e73 + ((vec3<f32>(1f, 1f, 1f) - _e73) * pow((1f - clamp(clamp(_e71.z, 0f, 1f), 0f, 1f)), 5f)));
                    let _e83 = clamp(dot(_e43, _e71), 0f, 1f);
                    let _e85 = max(clamp(_e42, 0f, 1f), 0.045f);
                    let _e86 = (_e85 * _e85);
                    let _e87 = (_e86 * _e86);
                    let _e91 = (((_e83 * _e83) * (_e87 - 1f)) + 1f);
                    let _e96 = (1f - _e87);
                    phi_158_ = (((((((_e63 * (vec3<f32>(1f, 1f, 1f) - _e81)) * (1f - _e44)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e81 * (_e87 / ((3.1415927f * _e91) * _e91))) * (0.5f / ((_e68 * sqrt((((_e66 * _e66) * _e96) + _e87))) + (_e66 * sqrt((((_e68 * _e68) * _e96) + _e87))))))) * _e68) * max(_e61, vec3<f32>(0f, 0f, 0f))) * 1f);
                    break;
                }
            }
            let _e120 = phi_158_;
            local_1 = (_e46 + _e120);
            continue;
        } else {
            break;
        }
        continuing {
            let _e377 = local_1;
            phi_82_ = _e377;
            phi_85_ = (_e48 + 1u);
        }
    }
    let _e382 = local_2;
    phi_160_ = _e382;
    phi_163_ = 0u;
    loop {
        let _e124 = phi_160_;
        let _e126 = phi_163_;
        let _e128 = LightCounts.point_light_count;
        local_4 = _e124;
        if (_e126 < min(_e128, 256u)) {
            let _e134 = point_lights.member[_e126].position;
            let _e135 = (_e134 - _e36);
            let _e136 = dot(_e135, _e135);
            let _e140 = point_lights.member[_e126].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_185_ = true;
                    if !((_e140 <= 0f)) {
                        phi_185_ = (_e136 >= (_e140 * _e140));
                    }
                    let _e147 = phi_185_;
                    if _e147 {
                        phi_196_ = 0f;
                        break;
                    }
                    let _e149 = (_e136 / (_e140 * _e140));
                    let _e152 = max((1f - (_e149 * _e149)), 0f);
                    phi_196_ = ((_e152 * _e152) / max(_e136, 0.0001f));
                    break;
                }
            }
            let _e157 = phi_196_;
            phi_161_ = _e124;
            if (_e157 > 0f) {
                let _e159 = normalize(_e135);
                let _e163 = point_lights.member[_e126].intensity;
                let _e166 = _e38.xyz;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e169 = clamp(_e43.z, 0f, 1f);
                        let _e171 = clamp(dot(_e43, _e159), 0f, 1f);
                        if (_e171 <= 0f) {
                            phi_264_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e174 = normalize((vec3<f32>(0f, 0f, 1f) + _e159));
                        let _e176 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e166, vec3(_e44));
                        let _e184 = (_e176 + ((vec3<f32>(1f, 1f, 1f) - _e176) * pow((1f - clamp(clamp(_e174.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e186 = clamp(dot(_e43, _e174), 0f, 1f);
                        let _e188 = max(clamp(_e42, 0f, 1f), 0.045f);
                        let _e189 = (_e188 * _e188);
                        let _e190 = (_e189 * _e189);
                        let _e194 = (((_e186 * _e186) * (_e190 - 1f)) + 1f);
                        let _e199 = (1f - _e190);
                        phi_264_ = (((((((_e166 * (vec3<f32>(1f, 1f, 1f) - _e184)) * (1f - _e44)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e184 * (_e190 / ((3.1415927f * _e194) * _e194))) * (0.5f / ((_e171 * sqrt((((_e169 * _e169) * _e199) + _e190))) + (_e169 * sqrt((((_e171 * _e171) * _e199) + _e190))))))) * _e171) * (max(_e163, vec3<f32>(0f, 0f, 0f)) * _e157)) * 1f);
                        break;
                    }
                }
                let _e223 = phi_264_;
                phi_161_ = (_e124 + _e223);
            }
            let _e226 = phi_161_;
            local_3 = _e226;
            continue;
        } else {
            break;
        }
        continuing {
            let _e384 = local_3;
            phi_160_ = _e384;
            phi_163_ = (_e126 + 1u);
        }
    }
    let _e392 = local_4;
    phi_267_ = _e392;
    phi_270_ = 0u;
    loop {
        let _e229 = phi_267_;
        let _e231 = phi_270_;
        let _e233 = LightCounts.spot_light_count;
        local = _e229;
        if (_e231 < min(_e233, 256u)) {
            let _e239 = spot_lights.member[_e231].position;
            let _e240 = (_e239 - _e36);
            let _e241 = dot(_e240, _e240);
            let _e245 = spot_lights.member[_e231].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_292_ = true;
                    if !((_e245 <= 0f)) {
                        phi_292_ = (_e241 >= (_e245 * _e245));
                    }
                    let _e252 = phi_292_;
                    if _e252 {
                        phi_303_ = 0f;
                        break;
                    }
                    let _e254 = (_e241 / (_e245 * _e245));
                    let _e257 = max((1f - (_e254 * _e254)), 0f);
                    phi_303_ = ((_e257 * _e257) / max(_e241, 0.0001f));
                    break;
                }
            }
            let _e262 = phi_303_;
            let _e267 = spot_lights.member[_e231].direction;
            let _e271 = spot_lights.member[_e231].inner_angle_cosine;
            let _e275 = spot_lights.member[_e231].outer_angle_cosine;
            switch bitcast<i32>(0u) {
                default: {
                    let _e279 = dot(normalize(-(_e240)), normalize(_e267));
                    let _e280 = (_e271 - _e275);
                    if (_e280 <= 0f) {
                        phi_329_ = select(0f, 1f, (_e279 >= _e275));
                        break;
                    }
                    let _e286 = clamp(((_e279 - _e275) / _e280), 0f, 1f);
                    phi_329_ = ((_e286 * _e286) * (3f - (2f * _e286)));
                    break;
                }
            }
            let _e292 = phi_329_;
            let _e293 = (_e262 * _e292);
            phi_268_ = _e229;
            if (_e293 > 0f) {
                let _e295 = normalize(_e240);
                let _e299 = spot_lights.member[_e231].intensity;
                let _e302 = _e38.xyz;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e305 = clamp(_e43.z, 0f, 1f);
                        let _e307 = clamp(dot(_e43, _e295), 0f, 1f);
                        if (_e307 <= 0f) {
                            phi_398_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e310 = normalize((vec3<f32>(0f, 0f, 1f) + _e295));
                        let _e312 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e302, vec3(_e44));
                        let _e320 = (_e312 + ((vec3<f32>(1f, 1f, 1f) - _e312) * pow((1f - clamp(clamp(_e310.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e322 = clamp(dot(_e43, _e310), 0f, 1f);
                        let _e324 = max(clamp(_e42, 0f, 1f), 0.045f);
                        let _e325 = (_e324 * _e324);
                        let _e326 = (_e325 * _e325);
                        let _e330 = (((_e322 * _e322) * (_e326 - 1f)) + 1f);
                        let _e335 = (1f - _e326);
                        phi_398_ = (((((((_e302 * (vec3<f32>(1f, 1f, 1f) - _e320)) * (1f - _e44)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e320 * (_e326 / ((3.1415927f * _e330) * _e330))) * (0.5f / ((_e307 * sqrt((((_e305 * _e305) * _e335) + _e326))) + (_e305 * sqrt((((_e307 * _e307) * _e335) + _e326))))))) * _e307) * (max(_e299, vec3<f32>(0f, 0f, 0f)) * _e293)) * 1f);
                        break;
                    }
                }
                let _e359 = phi_398_;
                phi_268_ = (_e229 + _e359);
            }
            let _e362 = phi_268_;
            local_5 = _e362;
            continue;
        } else {
            break;
        }
        continuing {
            let _e394 = local_5;
            phi_267_ = _e394;
            phi_270_ = (_e231 + 1u);
        }
    }
    let _e365 = MaterialConstants.emissive;
    let _e368 = local;
    let _e369 = (_e368 + max(_e365, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e369.x, _e369.y, _e369.z, _e38.w);
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
