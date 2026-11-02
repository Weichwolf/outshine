#version 450
#extension GL_GOOGLE_include_directive : require
layout(std430, set = 0, binding = 0) readonly buffer Inputs { uint words[]; };
layout(set = 0, binding = 1) uniform sampler2D preparedAtlas;
layout(std140, set = 2, binding = 0) uniform Pushed {
  uint count;
  uint first;
  uint width;
  uint unused;
};

uvec4 fieldPageEntry(uint index) {
  uint at = count * 12u + 4u + index * 4u;
  return uvec4(words[at], words[at + 1u], words[at + 2u], words[at + 3u]);
}
#include "groundLitterPages.glsl"
layout(location = 0) out vec4 sampled;
layout(location = 1) out vec4 address;

void main() {
  uint index = uint(gl_FragCoord.x);
  if (index >= count) { discard; }
  uint at = index * 12u;
  vec2 position = uintBitsToFloat(uvec2(words[at], words[at + 1u]));
  uint head = count * 12u;
  FieldPageLayout shape = FieldPageLayout(words[head], words[head + 1u], words[head + 2u], words[head + 3u]);
  if (words[at + 11u] != 0u) {
    ivec3 missing;
    vec4 value;
    bool found = groundLitterPages(preparedAtlas, shape, position,
        uintBitsToFloat(uvec2(words[at + 2u], words[at + 3u])),
        uintBitsToFloat(uvec2(words[at + 4u], words[at + 5u])), value, missing);
    sampled = value;
    address = vec4(vec3(missing), found ? 1.0 : 0.0);
    return;
  }
  float value = 0.0;
  ivec3 missing;
  bool found = fieldPageSample(preparedAtlas, shape, position, words[at + 2u], value, missing);
  sampled = vec4(value, found ? 1.0 : 0.0, 0.0, 1.0);
  address = vec4(vec3(missing), 1.0);
}
