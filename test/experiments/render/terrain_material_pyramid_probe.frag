#version 450
#extension GL_GOOGLE_include_directive : require

layout(std430, set = 0, binding = 0) readonly buffer Palette {
  float data[];
};

#include "terrain_material_sampling.glsl"
const float kPi = 3.14159265358979323846;
#include "metalRoughBrdf.glsl"
#include "terrain_material_moments.glsl"

layout(std140, set = 2, binding = 0) uniform Pushed {
  uint count;
  uint first;
  uint width;
  uint unused;
};

layout(location = 0) out vec4 material;
layout(location = 1) out vec4 gradientAddress;

vec3 shadeMaterial(Sample selected, uint mode) {
  vec3 n = normalize(vec3(-selected.GradientAddress.xy, 1.0));
  vec3 l = normalize(mode == 1u ? vec3(0.5, -0.2, 0.8) : vec3(-0.4, 0.2, 0.8));
  vec3 v = normalize(vec3(0.0, 0.6, 1.0));
  vec3 h = normalize(l + v);
  float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 1.0e-6);
  float roughness = clamp(selected.Material.a, 0.04, 1.0);
  float alpha = roughness * roughness;
  Brdf brdf = metalRoughBrdf(clamp(selected.Material.rgb, 0.0, 1.0),
                             vec3(0.04),
                             1.0,
                             alpha * alpha,
                             nl,
                             nv,
                             max(dot(n, h), 0.0),
                             max(dot(v, h), 0.0));
  return (brdf.diffuse + brdf.specular) * nl;
}

void main() {
  uint index = uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x);
  if (index >= count) { discard; }
  uint probeAt = word(319u) + (first + index) * 8u;
  int which = floatBitsToInt(data[probeAt + 6u]);
  if (which < 0 || uint(which) >= word(323u)) {
    material = vec4(0.0);
    gradientAddress = vec4(0.0);
    return;
  }
  uint encoded = word(probeAt + 7u);
  uint mode = encoded & 255u;
  if (mode == 3u || mode == 4u) {
    material = vec4(shadeMoments(which, mode), 1.0);
    gradientAddress = vec4(data[probeAt + 4u], data[probeAt + 5u], 0.0, 0.0);
    return;
  }
  vec4 uniforms = readFour(probeAt);
  Sample selected = sampleMaterial(uniforms, which);
  material = selected.Material;
  gradientAddress = selected.GradientAddress;
  if (mode == 1u || mode == 2u) {
    uint samples = clamp(encoded >> 8u, 1u, 4u);
    const vec4 offsets[4] =
        vec4[](vec4(0.0), vec4(0.5), vec4(0.75, 0.25, 0.25, 0.25), vec4(0.25, 0.75, 0.75, 0.75));
    vec3 sum = shadeMaterial(selected, mode);
    for (uint sampleIndex = 1u; sampleIndex < samples; ++sampleIndex) {
      sum += shadeMaterial(sampleMaterial(fract(uniforms + offsets[sampleIndex]), which), mode);
    }
    material = vec4(sum / float(samples), 1.0);
    gradientAddress = vec4(data[probeAt + 4u], data[probeAt + 5u], 0.0, 0.0);
  }
}
