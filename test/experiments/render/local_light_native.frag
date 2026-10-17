#version 450
#extension GL_GOOGLE_include_directive : require
#ifndef LOCAL_LIGHT_COUNTER
#define LOCAL_LIGHT_COUNTER 1
#endif
#include "localLightMaterial.glsl"
#include "lighting.glsl"
#include "brdfTables.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "sheenLobe.glsl"
#include "iridescenceLobe.glsl"
#include "microfacetEnergy.glsl"

layout(set = 2, binding = 0) uniform sampler2D shadowMap;
layout(set = 2, binding = 24, std430) readonly buffer Emitters { Light emitters[]; };
layout(set = 2, binding = 25, std430) readonly buffer Lists { uint words[]; };
layout(set = 3, binding = 4, std140) uniform ClusterFrame {
  uvec4 grid;
  vec4 depth;
  vec4 viewZ;
  vec4 viewport;
} clusters;
layout(set = 3, binding = 5, std140) uniform ProbeFrame {
  vec4 right;
  vec4 up;
  vec4 forward;
  vec4 eye;
  vec4 control;
} probe;
layout(location = 0) out vec4 colour;
uint supportCount = 0;

uvec2 lightSpan(vec3 p) {
  float viewDepth = dot(vec4(p, 1.0), clusters.viewZ);
  uint slice = 0;
  for (uint i = 1; i < clusters.grid.z; ++i) {
    if (viewDepth >= uintBitsToFloat(words[words[3] + i])) { slice = i; }
  }
  uvec2 tile = min(uvec2((gl_FragCoord.xy - clusters.viewport.xy) / float(clusters.grid.w)),
                   clusters.grid.xy - 1);
  uint cell = (slice * clusters.grid.y + tile.y) * clusters.grid.x + tile.x;
  uint header = words[2] + cell * 2;
  return uvec2(words[header], words[header + 1]);
}

uint lightIndexAt(uvec2 span, int at) {
  uint i = uint(at);
  return i < words[1] ? words[words[0] + i] : words[span.x + i - words[1]];
}

vec3 diffuseEnvironment(vec3 n) { return vec3(0.0); }
vec3 filteredEnvironment(vec3 direction, float roughness) { return vec3(0.0); }
#include "localLightSubject.glsl"

void main() {
  vec2 ndc = (gl_FragCoord.xy / clusters.viewport.zw * 2.0 - 1.0) * vec2(1.0, -1.0);
  float scale = probe.control.w > 0.5 ? probe.control.z : 1.0;
  vec3 p = probe.eye.xyz + probe.forward.xyz * probe.control.z +
           probe.right.xyz * (ndc.x * probe.control.x * scale) +
           probe.up.xyz * (ndc.y * probe.control.y * scale);
  vec3 n = -probe.forward.xyz;
  uint family = min(uint(gl_FragCoord.x * 8.0 / clusters.viewport.z), 7);
  M surface;
  surface.sheenColour = family == 2 ? vec3(0.3, 0.06, 0.1) : vec3(0.0);
  surface.sheenRoughness = 0.4;
  surface.clearcoat = family == 3 ? 0.7 : 0.0;
  surface.clearcoatRoughness = 0.2;
  surface.anisotropy = family == 4 ? 0.8 : 0.0;
  surface.anisotropyRotation = 0.3;
  surface.iridescence = family == 5 ? 0.8 : 0.0;
  surface.iridescenceThicknessMax = 400.0;
  surface.iridescenceIor = 1.3;
  surface.thinWeight = family == 6 ? 1.0 : (family == 7 ? 0.35 : 0.0);
  surface.thinReflection = vec3(0.15, 0.2, 0.1);
  surface.diffuseTransmission = vec3(0.3, 0.35, 0.25);
  surface.transmission = family == 7 ? 0.5 : 0.0;
  vec3 value = shadeRow(surface, p, n, p, vec3(0.45, 0.32, 0.16),
                        family == 1 ? 0.9 : 0.0, family == 1 ? 0.25 : 0.6,
                        vec3(0.04), 1.0, vec3(0.0), probe.right.xyz,
                        vec4(0.0, 0.0, 0.5, 1.0), shadowMap);
  colour = vec4(value, float(supportCount));
}
