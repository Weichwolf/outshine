#version 450
#extension GL_GOOGLE_include_directive : require
#define PRISM_VERTEX 1
#include "render/shaders/prismDifferentialVaryings.glsl"
struct Case { mat4 b; vec4 plane; vec4 pixel; };
layout(std430, set = 0, binding = 0) readonly buffer Cases { Case cases[]; };
layout(location = 0) flat out vec4 testPlane;
layout(location = 1) flat out vec4 testPixel;
layout(location = 2) flat out uint testIndex;
layout(location = 3) flat out uint testProjected;
void main() {
  uint id = uint(gl_InstanceIndex);
  Case entry = cases[id];
  const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(0, 1),
                                  vec2(1, 0), vec2(1, 1), vec2(0, 1));
  vec2 corner = corners[gl_VertexIndex];
  gl_Position = vec4(2.0 * (float(id) + corner.x) / float(cases.length()) - 1.0,
                      2.0 * corner.y - 1.0, 0.5, 1.0);
  testPlane = entry.plane;
  testPixel = entry.pixel;
  testIndex = id;
  testProjected = prismPlaneIncidence(entry.b, entry.plane.xyz, entry.plane.w) != 0.0 ? 1u : 0u;
  writePrismDifferentials(entry.b);
}
