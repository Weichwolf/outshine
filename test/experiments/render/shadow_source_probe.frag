#version 450
#extension GL_GOOGLE_include_directive : require
#define SUBJECT_BASE_LOCATION 0
#define LIT_TEXTURED 0
#define LIT_MAPPED 0
#define LIT_KIND 0
#include "material.glsl"
#include "subjectLights.glsl"
#include "brdfTables.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "sheenLobe.glsl"
#include "iridescenceLobe.glsl"
#include "microfacetEnergy.glsl"
#include "subjectLighting.glsl"
layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 colour;
void main() {
  M surface = materialAt();
  vec3 position = vec3(ndc.x, 0.0, ndc.y);
  vec4 lightSpace = vec4(ndc, 0.5 + 0.4 * ndc.x, 1.0);
  colour = vec4(shadeRow(surface, position, vec3(0.0, 1.0, 0.0), position,
      surface.base.rgb, surface.metalness, surface.roughness, surface.f0,
      surface.specularWeight, surface.emissive, vec3(0.0), lightSpace, shadowMap), 1.0);
}
