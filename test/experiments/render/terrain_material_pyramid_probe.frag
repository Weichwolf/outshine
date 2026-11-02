#version 450
#extension GL_GOOGLE_include_directive : require

layout(std430, set = 0, binding = 0) readonly buffer Palette {
  float data[];
};

#include "terrain_material_sampling.glsl"

layout(std140, set = 2, binding = 0) uniform Pushed {
  uint count;
  uint unused;
  uvec2 padding;
};

layout(location = 0) out vec4 material;
layout(location = 1) out vec4 gradientAddress;

void main() {
  uint index = uint(gl_FragCoord.x);
  if (index >= count) { discard; }
  uint probeAt = word(319u) + index * 8u;
  Sample selected = sampleMaterial(readFour(probeAt), floatBitsToInt(data[probeAt + 6u]));
  material = selected.Material;
  gradientAddress = selected.GradientAddress;
}
