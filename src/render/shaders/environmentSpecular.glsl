#include "environmentSampling.glsl"
layout(set = 2, binding = ENVIRONMENT_SPECULAR_BINDING) uniform sampler2D environmentMap;

vec3 environmentDirection(vec3 worldDirection) {
  vec3 up = normalize(lights.skyUp.xyz);
  vec3 sunward = lights.skyToSun.xyz - up * dot(up, lights.skyToSun.xyz);
  if (dot(sunward, sunward) < 1.0e-10) {
    vec3 pole = abs(up.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    sunward = cross(pole, up);
  }
  vec3 alongSun = normalize(sunward);
  return vec3(dot(worldDirection, alongSun), dot(worldDirection, cross(up, alongSun)),
              dot(worldDirection, up));
}

vec3 filteredEnvironment(vec3 reflection, float roughness) {
  float skyShare = clamp(dot(reflection, lights.up.xyz) * 0.5 + 0.5, 0.0, 1.0);
  vec3 authored = mix(lights.bounced.rgb, lights.environment.rgb, skyShare);
  if (lights.environment.w <= 0.0) { return authored; }
  vec3 direction = environmentDirection(reflection);
  float level = clamp(roughness, 0.0, 1.0) * float(kEnvironmentLevels - 1);
  float lower = floor(level);
  float upper = min(lower + 1.0, float(kEnvironmentLevels - 1));
  vec3 low = textureLod(environmentMap, environmentUv(direction, lower), 0.0).rgb;
  vec3 high = textureLod(environmentMap, environmentUv(direction, upper), 0.0).rgb;
  return authored + mix(low, high, level - lower) * lights.environment.w;
}
