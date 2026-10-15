#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 uv;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec3 position;
layout(location = 10) in vec3 groundWorldM;
layout(location = 11) in float facadeBay;
#include "material.glsl"
#include "surfaceGradient.glsl"
layout(location = 0) out vec4 outMaterial;
layout(location = 1) out vec4 outEstimate;
layout(location = 2) out vec4 outReference;
void main() {
  vec2 at = vec2(facadeBay, uv.y - 1.0);
  vec2 dx = dFdx(at), dy = dFdy(at);
  vec3 px = dFdx(position), py = dFdy(position);
  vec3 n = normalize(normal);
  vec3 gx = surfaceGradient(n, px, py, dx.x, dy.x);
  vec3 gy = surfaceGradient(n, px, py, dx.y, dy.y);
  vec2 metres = inversesqrt(max(vec2(dot(gx, gx), dot(gy, gy)), vec2(1.0e-20)));
  float bays = materialAt().base.r;
  vec3 physical = groundWorldM - vec3(7.31, 8.73, 4.27);
  vec2 reference = vec2(dot(physical, vec3(cos(0.43), 0.0, sin(0.43))), physical.y);
  reference += vec2(1.6 * bays, materialAt().base.g);
  outMaterial = materialAt().base;
  outEstimate = vec4(at * metres, 0.0, 1.0);
  outReference = vec4(reference, metres);
}
