#version 450
layout(location = 0) out float identity;
layout(location = 0) flat in uint primitiveIdentity;

layout(std430, set = 2, binding = 0) readonly buffer SceneDepth {
  float sceneDepth[];
};

layout(std140, set = 3, binding = 0) uniform Options {
  uvec4 options;
};

void main() {
  if (options.x > 0u) {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float reference = sceneDepth[pixel.y * 1280 + pixel.x];
    if (gl_FragCoord.z < reference) { discard; }
  }
  identity = float(primitiveIdentity);
}
