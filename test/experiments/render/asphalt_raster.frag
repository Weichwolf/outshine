#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#define GROUND_WORLD_POSITION
#include "subjectVaryings.glsl"
#include "material.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "asphalt_field.glsl"
layout(location = 0) out vec4 shaded;
layout(location = 1) out vec4 parameters;
layout(location = 2) out vec4 diagnostic;

void main() {
  M surface = materialAt();
  const float pitchM = 0.024;
  vec2 at = groundWorldM.xz / pitchM;
  float field =
      surface.pattern > 0.5
          ? asphaltFiltered(at, dFdx(groundWorldM.xz) / pitchM, dFdy(groundWorldM.xz) / pitchM)
          : 0.0;
  vec3 albedo = surface.base.rgb + 0.035 * field;
  float roughness = surface.roughness + 0.02 * field;
  vec3 n = normalize(normal);
  vec3 view = vec3(0.0, 1.0, 0.0);
  vec3 light = normalize(vec3(0.6, 0.8, 0.0));
  vec3 halfVector = normalize(view + light);
  float nl = max(dot(n, light), 0.0);
  float nv = max(dot(n, view), 1.0e-6);
  float nh = max(dot(n, halfVector), 0.0);
  float vh = max(dot(view, halfVector), 0.0);
  float alpha = roughness * roughness;
  Brdf brdf =
      brdfCombine(albedo, brdfFresnel(vec3(0.04), 1.0, vh), brdfLobe(alpha * alpha, nl, nv, nh));
  shaded = vec4((brdf.diffuse + brdf.specular) * nl, 1.0);
  parameters = vec4(albedo, roughness);
  diagnostic = vec4(at, field, 1.0);
}
