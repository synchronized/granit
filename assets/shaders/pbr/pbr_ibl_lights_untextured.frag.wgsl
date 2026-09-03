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
@group(1) @binding(0)
var<uniform> MaterialConstants: type_u002e_MaterialConstants;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> in_u002e_var_u002e_TEXCOORD3_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn fragment_main_1() {
    var phi_110_: vec3<f32>;
    var phi_113_: u32;
    var phi_181_: vec3<f32>;
    var phi_183_: vec3<f32>;
    var phi_186_: u32;
    var phi_208_: bool;
    var phi_219_: f32;
    var phi_282_: vec3<f32>;
    var phi_184_: vec3<f32>;
    var phi_285_: vec3<f32>;
    var phi_288_: u32;
    var phi_310_: bool;
    var phi_321_: f32;
    var phi_347_: f32;
    var phi_411_: vec3<f32>;
    var phi_286_: vec3<f32>;
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
    let _e53 = clamp(_e51.z, 0f, 1f);
    let _e54 = clamp(_e48, 0f, 1f);
    let _e55 = _e46.xyz;
    let _e57 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e55, vec3(_e54));
    let _e59 = MaterialConstants.occlusion_strength;
    phi_110_ = vec3<f32>(0f, 0f, 0f);
    phi_113_ = 0u;
    loop {
        let _e63 = phi_110_;
        let _e65 = phi_113_;
        let _e67 = LightCounts.directional_light_count;
        local_2 = _e63;
        if (_e65 < min(_e67, 16u)) {
            let _e73 = directional_lights.member[_e65].direction_to_light;
            let _e74 = normalize(_e73);
            let _e78 = directional_lights.member[_e65].radiance;
            switch bitcast<i32>(0u) {
                default: {
                    let _e82 = clamp(dot(_e51, _e74), 0f, 1f);
                    if (_e82 <= 0f) {
                        phi_181_ = vec3<f32>(0f, 0f, 0f);
                        break;
                    }
                    let _e85 = normalize((vec3<f32>(0f, 0f, 1f) + _e74));
                    let _e93 = (_e57 + ((vec3<f32>(1f, 1f, 1f) - _e57) * pow((1f - clamp(clamp(_e85.z, 0f, 1f), 0f, 1f)), 5f)));
                    let _e95 = clamp(dot(_e51, _e85), 0f, 1f);
                    let _e97 = max(clamp(_e50, 0f, 1f), 0.045f);
                    let _e98 = (_e97 * _e97);
                    let _e99 = (_e98 * _e98);
                    let _e103 = (((_e95 * _e95) * (_e99 - 1f)) + 1f);
                    let _e108 = (1f - _e99);
                    phi_181_ = (((((((_e55 * (vec3<f32>(1f, 1f, 1f) - _e93)) * (1f - _e54)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e93 * (_e99 / ((3.1415927f * _e103) * _e103))) * (0.5f / ((_e82 * sqrt((((_e53 * _e53) * _e108) + _e99))) + (_e53 * sqrt((((_e82 * _e82) * _e108) + _e99))))))) * _e82) * max(_e78, vec3<f32>(0f, 0f, 0f))) * 1f);
                    break;
                }
            }
            let _e132 = phi_181_;
            local_1 = (_e63 + _e132);
            continue;
        } else {
            break;
        }
        continuing {
            let _e442 = local_1;
            phi_110_ = _e442;
            phi_113_ = (_e65 + 1u);
        }
    }
    let _e447 = local_2;
    phi_183_ = _e447;
    phi_186_ = 0u;
    loop {
        let _e136 = phi_183_;
        let _e138 = phi_186_;
        let _e140 = LightCounts.point_light_count;
        local_4 = _e136;
        if (_e138 < min(_e140, 256u)) {
            let _e146 = point_lights.member[_e138].position;
            let _e147 = (_e146 - _e44);
            let _e148 = dot(_e147, _e147);
            let _e152 = point_lights.member[_e138].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_208_ = true;
                    if !((_e152 <= 0f)) {
                        phi_208_ = (_e148 >= (_e152 * _e152));
                    }
                    let _e159 = phi_208_;
                    if _e159 {
                        phi_219_ = 0f;
                        break;
                    }
                    let _e161 = (_e148 / (_e152 * _e152));
                    let _e164 = max((1f - (_e161 * _e161)), 0f);
                    phi_219_ = ((_e164 * _e164) / max(_e148, 0.0001f));
                    break;
                }
            }
            let _e169 = phi_219_;
            phi_184_ = _e136;
            if (_e169 > 0f) {
                let _e171 = normalize(_e147);
                let _e175 = point_lights.member[_e138].intensity;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e180 = clamp(dot(_e51, _e171), 0f, 1f);
                        if (_e180 <= 0f) {
                            phi_282_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e183 = normalize((vec3<f32>(0f, 0f, 1f) + _e171));
                        let _e191 = (_e57 + ((vec3<f32>(1f, 1f, 1f) - _e57) * pow((1f - clamp(clamp(_e183.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e193 = clamp(dot(_e51, _e183), 0f, 1f);
                        let _e195 = max(clamp(_e50, 0f, 1f), 0.045f);
                        let _e196 = (_e195 * _e195);
                        let _e197 = (_e196 * _e196);
                        let _e201 = (((_e193 * _e193) * (_e197 - 1f)) + 1f);
                        let _e206 = (1f - _e197);
                        phi_282_ = (((((((_e55 * (vec3<f32>(1f, 1f, 1f) - _e191)) * (1f - _e54)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e191 * (_e197 / ((3.1415927f * _e201) * _e201))) * (0.5f / ((_e180 * sqrt((((_e53 * _e53) * _e206) + _e197))) + (_e53 * sqrt((((_e180 * _e180) * _e206) + _e197))))))) * _e180) * (max(_e175, vec3<f32>(0f, 0f, 0f)) * _e169)) * 1f);
                        break;
                    }
                }
                let _e230 = phi_282_;
                phi_184_ = (_e136 + _e230);
            }
            let _e233 = phi_184_;
            local_3 = _e233;
            continue;
        } else {
            break;
        }
        continuing {
            let _e449 = local_3;
            phi_183_ = _e449;
            phi_186_ = (_e138 + 1u);
        }
    }
    let _e457 = local_4;
    phi_285_ = _e457;
    phi_288_ = 0u;
    loop {
        let _e236 = phi_285_;
        let _e238 = phi_288_;
        let _e240 = LightCounts.spot_light_count;
        local = _e236;
        if (_e238 < min(_e240, 256u)) {
            let _e246 = spot_lights.member[_e238].position;
            let _e247 = (_e246 - _e44);
            let _e248 = dot(_e247, _e247);
            let _e252 = spot_lights.member[_e238].radius;
            switch bitcast<i32>(0u) {
                default: {
                    phi_310_ = true;
                    if !((_e252 <= 0f)) {
                        phi_310_ = (_e248 >= (_e252 * _e252));
                    }
                    let _e259 = phi_310_;
                    if _e259 {
                        phi_321_ = 0f;
                        break;
                    }
                    let _e261 = (_e248 / (_e252 * _e252));
                    let _e264 = max((1f - (_e261 * _e261)), 0f);
                    phi_321_ = ((_e264 * _e264) / max(_e248, 0.0001f));
                    break;
                }
            }
            let _e269 = phi_321_;
            let _e274 = spot_lights.member[_e238].direction;
            let _e278 = spot_lights.member[_e238].inner_angle_cosine;
            let _e282 = spot_lights.member[_e238].outer_angle_cosine;
            switch bitcast<i32>(0u) {
                default: {
                    let _e286 = dot(normalize(-(_e247)), normalize(_e274));
                    let _e287 = (_e278 - _e282);
                    if (_e287 <= 0f) {
                        phi_347_ = select(0f, 1f, (_e286 >= _e282));
                        break;
                    }
                    let _e293 = clamp(((_e286 - _e282) / _e287), 0f, 1f);
                    phi_347_ = ((_e293 * _e293) * (3f - (2f * _e293)));
                    break;
                }
            }
            let _e299 = phi_347_;
            let _e300 = (_e269 * _e299);
            phi_286_ = _e236;
            if (_e300 > 0f) {
                let _e302 = normalize(_e247);
                let _e306 = spot_lights.member[_e238].intensity;
                switch bitcast<i32>(0u) {
                    default: {
                        let _e311 = clamp(dot(_e51, _e302), 0f, 1f);
                        if (_e311 <= 0f) {
                            phi_411_ = vec3<f32>(0f, 0f, 0f);
                            break;
                        }
                        let _e314 = normalize((vec3<f32>(0f, 0f, 1f) + _e302));
                        let _e322 = (_e57 + ((vec3<f32>(1f, 1f, 1f) - _e57) * pow((1f - clamp(clamp(_e314.z, 0f, 1f), 0f, 1f)), 5f)));
                        let _e324 = clamp(dot(_e51, _e314), 0f, 1f);
                        let _e326 = max(clamp(_e50, 0f, 1f), 0.045f);
                        let _e327 = (_e326 * _e326);
                        let _e328 = (_e327 * _e327);
                        let _e332 = (((_e324 * _e324) * (_e328 - 1f)) + 1f);
                        let _e337 = (1f - _e328);
                        phi_411_ = (((((((_e55 * (vec3<f32>(1f, 1f, 1f) - _e322)) * (1f - _e54)) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + ((_e322 * (_e328 / ((3.1415927f * _e332) * _e332))) * (0.5f / ((_e311 * sqrt((((_e53 * _e53) * _e337) + _e328))) + (_e53 * sqrt((((_e311 * _e311) * _e337) + _e328))))))) * _e311) * (max(_e306, vec3<f32>(0f, 0f, 0f)) * _e300)) * 1f);
                        break;
                    }
                }
                let _e361 = phi_411_;
                phi_286_ = (_e236 + _e361);
            }
            let _e364 = phi_286_;
            local_5 = _e364;
            continue;
        } else {
            break;
        }
        continuing {
            let _e459 = local_5;
            phi_285_ = _e459;
            phi_288_ = (_e238 + 1u);
        }
    }
    let _e366 = clamp(_e50, 0f, 1f);
    let _e375 = (_e57 + ((max(vec3((1f - _e366)), _e57) - _e57) * pow((1f - clamp(_e53, 0f, 1f)), 5f)));
    let _e380 = IblConstants.environment_rotation_cos;
    let _e384 = IblConstants.environment_rotation_sin;
    let _e388 = -(_e384);
    let _e393 = textureSample(irradiance_texture, environment_sampler, vec3<f32>(((_e380 * _e51.x) + (_e384 * _e51.z)), _e51.y, ((_e388 * _e51.x) + (_e380 * _e51.z))));
    let _e395 = reflect(vec3<f32>(-0f, -0f, -1f), _e51);
    let _e407 = IblConstants.prefiltered_environment_max_mip;
    let _e409 = textureSampleLevel(prefiltered_environment_texture, environment_sampler, vec3<f32>(((_e380 * _e395.x) + (_e384 * _e395.z)), _e395.y, ((_e388 * _e395.x) + (_e380 * _e395.z))), (_e366 * _e407));
    let _e412 = textureSample(brdf_lut_texture, environment_sampler, vec2<f32>(_e53, _e366));
    let _e424 = IblConstants.environment_intensity;
    let _e429 = local;
    let _e432 = MaterialConstants.emissive;
    let _e434 = ((_e429 + ((((((((vec3<f32>(1f, 1f, 1f) - _e375) * (1f - _e54)) * _e55) * _e393.xyz) * vec3<f32>(0.31830987f, 0.31830987f, 0.31830987f)) + (_e409.xyz * ((_e375 * _e412.x) + vec3(_e412.y)))) * max(_e424, 0f)) * mix(1f, 1f, clamp(_e59, 0f, 1f)))) + max(_e432, vec3<f32>(0f, 0f, 0f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e434.x, _e434.y, _e434.z, _e46.w);
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
